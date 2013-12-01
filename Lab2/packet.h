const int PACKET_SIZE = 1024;
const int HEADER_SIZE = 16;
const int PAYLOAD_SIZE = PACKET_SIZE - HEADER_SIZE;
const int ACK_SIZE = 16;
const int TIMEOUT = 2;

struct FilePacket{
	unsigned long source_port;
	unsigned long dest_port;
	unsigned long seq_num;
	unsigned short payload_size; //Necessary if it is less than default
	char payload[PAYLOAD_SIZE];
};

struct AckPacket{
	unsigned long source_port;
	unsigned long dest_port;
	unsigned long ack_num;
	unsigned long file_offset; //Next part of file needed
};
