#include <sys/time.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

const char *server_type = "server_kernel v" VERSION;
const char *pid_file = PID_DIR "/server_kernel.pid";
char *password = NULL;

#include "error.h"
#include "utils.h"
#include "log.h"
#include "protocol.h"
#include "server_utils.h"
#include "auth.h"
#include "kernel_prng_rw.h"

void sig_handler(int sig)
{
	fprintf(stderr, "Exit due to signal %d\n", sig);
	unlink(pid_file);
	exit(0);
}

void help(void)
{
	printf("-i host   entropy_broker-host to connect to\n");
	printf("-o file   file to write entropy data to\n");
	printf("-S        show bps (mutual exclusive with -n)\n");
	printf("-l file   log to file 'file'\n");
	printf("-s        log to syslog\n");
	printf("-n        do not fork\n");
	printf("-P file   write pid to file\n");
	printf("-X file   read password from file\n");
}

int main(int argc, char *argv[])
{
	char *host = NULL;
	int port = 55225;
	int socket_fd = -1;
	int c;
	char do_not_fork = 0, log_console = 0, log_syslog = 0;
	char *log_logfile = NULL;
	char *bytes_file = NULL;
	char show_bps = 0;
	long int total_byte_cnt = 0;

	fprintf(stderr, "%s, (C) 2009-2012 by folkert@vanheusden.com\n", server_type);
	printf("Please note: this program RETRIEVES entropy data from the kernel and feeds that to the entropybroker!\n");
	printf("If you want to ADD data to the kernel entropy buffer instead (which is what you most likely want to do), then use eb_client_linux_kernel\n");

	while((c = getopt(argc, argv, "X:P:So:i:l:sn")) != -1)
	{
		switch(c)
		{
			case 'X':
				password = get_password_from_file(optarg);
				break;

			case 'P':
				pid_file = optarg;
				break;

			case 'S':
				show_bps = 1;
				break;

			case 'o':
				bytes_file = optarg;
				break;

			case 'i':
				host = optarg;
				break;

			case 's':
				log_syslog = 1;
				break;

			case 'l':
				log_logfile = optarg;
				break;

			case 'n':
				do_not_fork = 1;
				log_console = 1;
				break;

			default:
				help();
				return 1;
		}
	}

	if (!password)
		error_exit("no password set");
	set_password(password);

	if (!host && !bytes_file && show_bps == 0)
		error_exit("no host to connect to/file to write to given");

	set_logging_parameters(log_console, log_logfile, log_syslog);

	if (!do_not_fork && !show_bps)
	{
		if (daemon(-1, -1) == -1)
			error_exit("fork failed");
	}

	write_pid(pid_file);

	signal(SIGPIPE, SIG_IGN);
	signal(SIGTERM, sig_handler);
	signal(SIGINT , sig_handler);
	signal(SIGQUIT, sig_handler);

	double cur_start_ts = get_ts();
	for(;;)
	{
		if (host != NULL)
		{
			if (reconnect_server_socket(host, port, password, &socket_fd, server_type, 1) == -1)
				continue;

			disable_nagle(socket_fd);
			enable_tcp_keepalive(socket_fd);
		}

		unsigned char bytes[1249];
		if (kernel_rng_read_blocking(bytes, sizeof bytes) == -1)
		{
			dolog(LOG_WARNING, "Problem reading from kernel entropy buffer!");
			sleep(1);
		}

		if (bytes_file)
		{
			emit_buffer_to_file(bytes_file, bytes, sizeof bytes);
		}
		if (host)
		{
			if (message_transmit_entropy_data(socket_fd, bytes, sizeof bytes) == -1)
			{
				dolog(LOG_INFO, "connection closed");

				close(socket_fd);
				socket_fd = -1;
			}
		}

		if (show_bps)
		{
			double now_ts = get_ts();

			total_byte_cnt += sizeof bytes;

			if ((now_ts - cur_start_ts) >= 1.0)
			{
				int diff_t = now_ts - cur_start_ts;

				printf("Total number of bytes: %ld, avg/s: %f\n", total_byte_cnt, double(total_byte_cnt) / diff_t);

				cur_start_ts = now_ts;
				total_byte_cnt = 0;
			}
		}
	}

	unlink(pid_file);

	return 0;
}