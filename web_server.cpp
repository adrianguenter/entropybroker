#include <errno.h>
#include <map>
#include <pthread.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <unistd.h>
#include <vector>
#include <sys/socket.h>
#include <sys/types.h>

#include "error.h"
#include "log.h"
#include "utils.h"
#include "http_request_t.h"
#include "http_bundle.h"
#include "http_file.h"
#include "http_file_version.h"
#include "http_server.h"
#include "web_server.h"

typedef struct
{
	web_server *p_server;
	int fd;
} client_t;

void *start_web_server_thread_wrapper(void *p)
{
	web_server *ws = reinterpret_cast<web_server *>(p);

	ws -> run();

	return NULL;
}

void start_web_server(std::string listen_adapter, int listen_port)
{
	web_server *ws = new web_server(listen_adapter, listen_port);

	pthread_t thread;
	pthread_check(pthread_create(&thread, NULL, start_web_server_thread_wrapper, ws), "pthread_create");
}

web_server::web_server(std::string listen_adapter, int listen_port)
{
	fd = start_listen(listen_adapter.c_str(), listen_port, 64);

	add_object(new http_file_version());
}

web_server::~web_server()
{
	if (fd != -1)
		close(fd);

	std::map<std::string, http_file *>::iterator it = objects.begin();
	while(it != objects.end())
	{
		delete it -> second;
		it++;
	}
}

void web_server::add_object(http_file *p)
{
	objects.insert(std::pair<std::string, http_file *>(p -> get_url(), p));
}

void * thread_wrapper_http_server(void *thread_data)
{
	client_t *p_data = reinterpret_cast<client_t *>(thread_data);

	http_server *hs = new http_server(p_data -> fd);

	// get url
	std::string url = hs -> get_request_url();

	// get request type
	http_request_t request_type = hs -> get_request_type();

	// get request_details
	http_bundle *request_details = hs -> get_request();

	// lookup_url -> file
	http_file *obj = p_data -> p_server -> lookup_url(url); // not allocated, don't free it

	http_bundle *response = obj -> do_request(request_type, request_details);

	std::vector<std::string> headers;
	headers.push_back(("Content-Type: " + obj -> get_meta_type()).c_str());

	hs -> send_response(200, &headers, response);

	delete response;

	delete request_details;

	delete hs;

	close(p_data -> fd);

	delete p_data;

	return NULL;
}

void web_server::run(void)
{
	for(;;)
	{
		int client_fd = accept(fd, NULL, NULL);

		if (client_fd == -1)
		{
			dolog(LOG_INFO, "web_server: accept failed: %s", strerror(errno));

			continue;
		}

		std::string host = get_endpoint_name(client_fd);

		dolog(LOG_INFO, "web_server: connected with %s", host.c_str());

		client_t *p_client = new client_t;
		p_client -> p_server = this;
		p_client -> fd = client_fd;

		pthread_check(pthread_create(thread, NULL, thread_wrapper_http_server, reinterpret_cast<void *>(p_client)), "pthread_create");
	}
}

http_file * web_server::lookup_url(std::string url)
{
	std::map<std::string, http_file *>::iterator index = objects.find(url);

	if (index != objects.end())
		return index -> second;

	return NULL;
}
