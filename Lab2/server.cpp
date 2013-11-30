#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <ctime>
#include <strings.h>
#include <string.h>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include "packet.h"
#include <bitset>
using namespace std;

void error(string msg)
{
	perror(msg.c_str());
	exit(1);
}

void setPacketHeader(FilePacket *packet, unsigned long packet_num, struct sockaddr_in &client, int portno, int cwnd)
{
	packet->source_port = portno;
	packet->dest_port = ntohs(client.sin_port);
	packet->seq_num = packet_num;
	packet->window_size = cwnd;
}

void sendFile(int sock, int portno, struct sockaddr_in &client, ifstream &input, unsigned long file_size, int cwnd, float p_loss, float p_cor)
{
	int n;
	socklen_t clilen = sizeof(client);
	FilePacket packet;
	AckPacket ack_packet;
	long count = PAYLOAD_SIZE;
	unsigned long packet_num = 1;
	
	while(count < file_size){
		//Read input
		bzero(&packet, PACKET_SIZE);
		setPacketHeader(&packet, packet_num, client, portno, cwnd); 	
		input.read(packet.payload, PAYLOAD_SIZE);

		bool corruption = true;
		//Wait for ack packet, retransmit if timeout
		while(corruption){
			//Emulate packet loss by not transmitting packet if random is less than p_loss 
			if(random() % 101 >= p_loss * 100){		
				n = sendto(sock, (char *)&packet, PACKET_SIZE, 0, (struct sockaddr *) &client, clilen); 
			}else{
				printf("Packet %lu will be lost\n", packet_num);
			}

			bzero(&ack_packet, ACK_SIZE);
			n = recvfrom(sock, (char *)&ack_packet, ACK_SIZE, 0, (struct sockaddr *) &client, &clilen);	
			if(n < 0){
				printf("Timeout, resending packet %lu...\n", packet_num);
				continue;
			}
			
			//Emulate ack corruption
			if(random() % 101 < p_cor * 100){
				printf("Ack packet %lu is corrupted\n", ack_packet.ack_num);
				printf("Resending packet %lu...\n", packet_num);
				continue;
			}
			corruption = false;
		}

		printf("Client acknowledged recieving packet %lu\n", ack_packet.ack_num);
		count += PAYLOAD_SIZE;
		packet_num++;
	}

	bool corruption = true;
	//Calculate remaining file that hasn't been sent
	if(file_size > PAYLOAD_SIZE)
		count = PAYLOAD_SIZE - count % file_size;
	else
		count = file_size;

	//Zero out packet and read from input
	bzero(&packet, PACKET_SIZE);
	setPacketHeader(&packet, packet_num, client, portno, cwnd);
	input.read(packet.payload, count);
	
	while(corruption){
		//Emulate packet loss by not transmitting packet if random is less than p_loss 
		if(random() % 101 >= p_loss * 100){		
			n = sendto(sock, (char *)&packet, PACKET_SIZE, 0, (struct sockaddr *) &client, clilen); 
		}else{
			printf("Packet %lu will be lost\n", packet_num);
		}
	
		//Wait for ack or retransmit	
		bzero(&ack_packet, ACK_SIZE);
		n = recvfrom(sock, (char *)&ack_packet, ACK_SIZE, 0, (struct sockaddr *) &client, &clilen);
		if(n < 0){
			continue;
		}
	
		//Emulate packet corruption
		if(random() % 101 < p_cor * 100){
			printf("Ack packet %lu is corrupted\n", ack_packet.ack_num);
			printf("Resending packet %lu...\n", packet_num);
			continue;
		}
	
		printf("Client acknowledged recieving packet %lu\n", ack_packet.ack_num);
		corruption = false;
	}
}

void parseRequest(int sock, int portno, struct sockaddr_in &client, char *path, int cwnd, float p_loss, float p_cor)
{
	int n;
	socklen_t clilen = sizeof(client);
	ifstream file;
	string msg;
	file.open(path);

	//Find out file size and reset file position
	file.seekg(0, ifstream:: end);
	stringstream ss;
	unsigned long size = file.tellg();
	ss << file.tellg();
	file.clear();
    file.seekg(0, ifstream::beg);

	//Send response message to client and start sending file	
	if(file.is_open()){
		msg = ss.str();
		n = sendto(sock, msg.c_str(), msg.length(), 0, (struct sockaddr *) &client, clilen);
		printf("Sending file to client...\n");
		sendFile(sock, portno, client, file, size, cwnd, p_loss, p_cor);
		printf("Successfully sent the file!\n");
	}else{
		printf("File not found\n");
		msg = "File not found";
		n = sendto(sock, msg.c_str(), msg.length(), 0, (struct sockaddr *) &client, clilen);
	}
	file.close();
}	

int main(int argc, char *argv[])
{
	int sockfd, portno, cwnd;
	float p_loss, p_cor;
	socklen_t clilen;
	struct sockaddr_in serv_addr, cli_addr;
	srand(time(NULL));

	if(argc < 4){
		fprintf(stderr, "ERROR need port, window size, loss prob, and corr prob in that order\n");
		exit(1);	
	}	
	portno = atoi(argv[1]);
	cwnd = atoi(argv[2]);
	p_loss = atof(argv[3]);
	p_cor = atof(argv[4]);
	
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if(sockfd < 0){
		error("ERROR opening socket");
	}		

	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(portno);
	serv_addr.sin_addr.s_addr = INADDR_ANY;			

	if(bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0){
		error("ERROR on binding");	
	}	

	listen(sockfd, 5);

	//Waiting for request from client
	clilen = sizeof(cli_addr);
	
	//Set timeout on all requests
	struct timeval tv;
	tv.tv_sec = 2;
	tv.tv_usec = 0;
	setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(struct timeval));

	while(1){
		int n;
		char buffer[256];
		bzero(buffer, 256);
				
		n = recvfrom(sockfd, buffer, 255, 0, (struct sockaddr *) &cli_addr, &clilen);

		//Parse client request and send file	
		if(n >= 0){
			printf("Client requesting the file: %s\n", buffer);
			parseRequest(sockfd, portno, cli_addr, buffer, cwnd, p_loss, p_cor);
		}	
		//close(sockfd);	
	}	
}
