const int PACKET_SIZE = 8192;
const int HEADER_SIZE = 96;
const int PAYLOAD_SIZE = PACKET_SIZE - HEADER_SIZE;
const int ACK_SIZE = 80;

struct FilePacket{
	unsigned long source_port;
	unsigned long dest_port;
	unsigned long seq_num;
	unsigned short checksum;
	unsigned short window_size;
	char payload[PAYLOAD_SIZE];
};

struct AckPacket{
	unsigned long source_port;
	unsigned long dest_port;
	unsigned long ack_num;
	unsigned short checksum;
};
