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
#include <iostream>
#include "packet.h"
using namespace std;

void error(string msg)
{
	perror(msg.c_str());
	exit(1);	
}

void setAckHeader(AckPacket *packet, unsigned long packet_num, unsigned long offset, struct sockaddr_in &server, int portno)
{
	packet->source_port = portno;
	packet->dest_port = ntohs(server.sin_port);
	packet->ack_num = packet_num;
	packet->file_offset = offset;
}

void getFile(int sock, int portno, struct sockaddr_in &server, ofstream &output, unsigned long file_size, float p_loss, float p_cor)
{
	int n;
	socklen_t servlen = sizeof(server);
	FilePacket packet;
	AckPacket ack_packet;
	unsigned long file_offset = 0;
	unsigned long expected_packet = 0;
	while(file_offset < file_size){
		//Retrieve packet from server, keep waiting to account for actual packet loss
		bzero(&packet, PACKET_SIZE);
		n = recvfrom(sock, (char *) &packet, PACKET_SIZE, 0, (struct sockaddr *) &server, &servlen);
		if(n < 0){
			continue;
		}
	
		//Simulate packet loss	
		if(random() % 100 < p_loss * 100){
			printf("Packet %lu loss\n", packet.seq_num);
			continue;
		}
		
		//Simulate packet corruption, discard packet then wait for retransmit
		if(random() % 100 < p_cor * 100){
			printf("Packet %lu corruption, discarding...\n", packet.seq_num);					
			continue;	
		}

		//Make sure the packet is expected	
		if(packet.seq_num == expected_packet)	
			printf("Recieved packet %lu\n", packet.seq_num);
		else
			printf("Duplicate or out of order packet, discarding...\n");

		//Send packet acknowledgement
		bzero(&ack_packet, ACK_SIZE);
		if(packet.seq_num == expected_packet){
			setAckHeader(&ack_packet, expected_packet+1, file_offset + packet.payload_size, server, portno);
		}else{
			setAckHeader(&ack_packet, expected_packet, file_offset, server, portno);
		}
		n = sendto(sock, (char *) &ack_packet, ACK_SIZE, 0, (struct sockaddr *) &server, servlen);
		printf("Sent ack %lu\n", ack_packet.ack_num);
		
		//If duplicate packet dont write to output
		if(packet.seq_num != expected_packet){
			continue;
		}
		output.write(packet.payload, packet.payload_size);
		expected_packet++;
		file_offset += packet.payload_size;
	}

	printf("Finished downloading file!\n");
	//If last ack packet is lost then keep resending ack till server stops sending packets
	bool finished = false;
	while(!finished){
		n = recvfrom(sock, (char *) &packet, PACKET_SIZE, 0, (struct sockaddr *) &server, &servlen);
		if(n >= 0){
			//Simulating packet loss
			if(random() % 100 < p_loss * 100){
				printf("Packet %lu loss\n", packet.seq_num);
				continue;
			}
			printf("Duplicate or out of order packet, discarding...\n");
			bzero(&ack_packet, ACK_SIZE);
			setAckHeader(&ack_packet, expected_packet, file_size, server, portno);
			n = sendto(sock, (char *) &ack_packet, ACK_SIZE, 0, (struct sockaddr *) &server, servlen);
			printf("Sent ack %lu\n", ack_packet.ack_num);
			continue;
		}
		finished = true;
	}
}

int main(int argc, char *argv[])
{
	//Initialize connection
	int sockfd, portno, n;
	float p_loss, p_cor;
	socklen_t serv_len;
	struct sockaddr_in serv_addr, cli_addr;
	struct hostent *server;
	string file_name;
	srand(time(NULL));

	if(argc < 6){
		fprintf(stderr, "ERROR need hostname, port number, filename, loss prob, and corr prob in that order\n");
		exit(1);	
	}		

	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if(sockfd < 0){
		error("ERROR opening socket");
	}	
	server = gethostbyname(argv[1]);
	if(server == NULL){
		fprintf(stderr, "ERROR, no such host\n");	
		exit(0);	
	}
	portno = atoi(argv[2]);
	file_name = argv[3];
	p_loss = atof(argv[4]);
	p_cor = atof(argv[5]);

	bzero((char *) &serv_addr, sizeof(serv_addr));		
	serv_addr.sin_family = AF_INET;
	bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
	serv_addr.sin_port = htons(portno);	

	serv_len = sizeof(serv_addr);

	//Set timeout on all requests
	struct timeval tv;
	tv.tv_sec = TIMEOUT * 2;
	tv.tv_usec = 0; //Error without init
	setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(struct timeval));

	//Send file request to server
	n = sendto(sockfd, file_name.c_str(), file_name.length(), 0, (struct sockaddr *) &serv_addr, serv_len);

	//Recieve reseponse from server, includes file size or error message	
	char buffer[256];
	bzero(buffer, 256);
	n = recvfrom(sockfd, buffer, 255, 0, (struct sockaddr *) &serv_addr, &serv_len);

	//If file is not found on server exit
	string msg(buffer);
	if(msg == "File not found"){
		printf("File not found on server\n");
		exit(0);
	}

	//Get file from server given the size of the file
	unsigned long file_size = strtoul(buffer, NULL, 0);
	printf("Retrieving file(%lubytes)...\n", file_size);
	
	file_name = file_name.substr(file_name.find_last_of("/")+1);
	ofstream file;
	file.open(file_name.c_str()); 
	if(file.is_open())
		getFile(sockfd, portno, serv_addr, file, file_size, p_loss, p_cor);
	else
		error("Error creating new file");
	file.close();	
}
