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

void setPacketHeader(FilePacket *packet, unsigned long packet_num, int size, struct sockaddr_in &client, int portno)
{
	packet->source_port = portno;
	packet->dest_port = ntohs(client.sin_port);
	packet->seq_num = packet_num;
	packet->payload_size = size;
}

void sendFile(int sock, int portno, struct sockaddr_in &client, ifstream &input, unsigned long file_size, int cwnd, float p_loss, float p_cor)
{
	int n = -1;
	socklen_t clilen = sizeof(client);
	FilePacket packet;
	AckPacket ack_packet;
	unsigned long packet_num = 0;
	unsigned long last_acked = 0;
	unsigned long file_offset = 0;
	
	while(file_offset < file_size){
		int window_offset = 0;
		int packets_sent = 0;
		unsigned long seek_offset = file_offset;

		while(window_offset < cwnd && seek_offset < file_size){
			input.seekg(0, ifstream::beg);
			
			//Set payload size, care for excess bytes when window_offset is > cwnd	
			unsigned long payload_size = PAYLOAD_SIZE;
			if(window_offset + PACKET_SIZE > cwnd){
				payload_size = PACKET_SIZE - (window_offset + PACKET_SIZE) % cwnd;
				if(payload_size <= HEADER_SIZE)
					break;
				else
					payload_size -= HEADER_SIZE;		
			}
			//Check and account for end of file
			if(seek_offset + payload_size > file_size){
				payload_size = file_size - seek_offset; 
			}

			//Read input of file, reset seekg to current file offset	
			bzero(&packet, PACKET_SIZE);
			setPacketHeader(&packet, packet_num, payload_size, client, portno);
			input.seekg(seek_offset);
			input.read(packet.payload, payload_size);

			//Send packet to client		
			n = sendto(sock, (char *)&packet, PACKET_SIZE, 0, (struct sockaddr *) &client, clilen); 
			printf("Sent packet %lu\n", packet.seq_num);	

			packet_num++;
			packets_sent++;
			window_offset += PACKET_SIZE;
			seek_offset += payload_size;
		}
	
		//Wait for same number of acks as packets sent	
		for(int i = 0; i < packets_sent; i++){
			bzero(&ack_packet, ACK_SIZE);
			n = recvfrom(sock, (char *)&ack_packet, ACK_SIZE, 0, (struct sockaddr *) &client, &clilen);	
			//Update values if recieved ack, if timeout then stop waiting for ack
			if(n >= 0){
				//Simulate ack corruption and ack loss by discarding ack
				if(random() % 100 < p_loss * 100){
					printf("Ack %lu loss\n", ack_packet.ack_num);
				}else if(random() % 100 < p_cor * 100){
					printf("Ack %lu corruption, discarding...\n", ack_packet.ack_num);
				}else{
					last_acked = ack_packet.ack_num;
					file_offset = ack_packet.file_offset;	
					printf("Recieved ack %lu\n", ack_packet.ack_num);
				}
			//Actual packet loss, just break out of loop to avoid waiting for multiple TIMEOUT 
			}else{
				break;
			}
		}
		packet_num = last_acked;
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
	tv.tv_sec = TIMEOUT;
	tv.tv_usec = 0; //Error without init
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
