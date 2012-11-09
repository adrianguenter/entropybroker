class http_file
{
public:
	http_file();
	virtual ~http_file();

	virtual std::string get_url() = 0;

	virtual std::string get_meta_type() = 0;

	std::map<std::string, std::string> split_parameters(std::string url);

	virtual http_bundle * do_request(http_request_t request_type, std::string request_url, http_bundle *request_details) = 0;
};