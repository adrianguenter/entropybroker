class stirrer_camellia : public stirrer
{
public:
	stirrer_camellia();
	~stirrer_camellia();

	int get_ivec_size() const;
	int get_stir_size() const;
	void do_stir(unsigned char *ivec, unsigned char *target, int target_size, unsigned char *data_in, int data_in_size, unsigned char *temp_buffer, bool direction);
};