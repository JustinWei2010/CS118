#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include "packet.h"
using namespace std;

void error(string msg)
{
	perror(msg.c_str());
	exit(1);
}

/*
	Set packet Header. Header will include source port(32 bits), dest port(32 bits),
    seq number(32 bits), checksum(16 bits), and window size(16 bits).
*/
void setPacketHeader(FilePacket *packet, unsigned long packet_num, struct sockaddr_in &client)
{
	packet->source_port = 10000;
	packet->dest_port = 10000;
	packet->seq_num = packet_num;
	packet->checksum = 0;
	packet->window_size = 0;
}

void sendFile(int sock, struct sockaddr_in &client, ifstream &input, unsigned long file_size)
{
	int n;
	socklen_t clilen = sizeof(client);
	char packet[PACKET_SIZE];
	long count = PACKET_SIZE;
	char ack_packet[256];	

	while(count < file_size){
		bzero(packet, PACKET_SIZE);
		bzero(ack_packet, 256);
		input.read(packet, PACKET_SIZE);
		n = sendto(sock, packet, PACKET_SIZE, 0, (struct sockaddr *) &client, clilen); 
		if(n < 0)
			printf("Error sending file to client\n");
		n = recvfrom(sock, ack_packet, 256, 0, (struct sockaddr *) &client, &clilen);	
		printf("Client acknowledged recieving packet\n");
		count += PACKET_SIZE;
	}

	if(file_size > PACKET_SIZE)
		count = PACKET_SIZE - count % file_size;
	else
		count = file_size;
	bzero(packet, PACKET_SIZE);
	bzero(ack_packet, 256);
	input.read(packet, count);
	n = sendto(sock, packet, count, 0, (struct sockaddr *) &client, clilen);
	if(n < 0)
		printf("Error sending file to client\n");
	n = recvfrom(sock, ack_packet, 256, 0, (struct sockaddr *) &client, &clilen);
	printf("Client acknowledged recieving packet\n");
}

void dostuff(int sock, struct sockaddr_in &client, char *path)
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
		sendFile(sock, client, file, size);
	}else{
		printf("File not found\n");
		msg = "File not found";
		n = sendto(sock, msg.c_str(), msg.length(), 0, (struct sockaddr *) &client, clilen);
	}
	file.close();
}	

int main(int argc, char *argv[])
{
	cout << sizeof(long) << endl;
	int sockfd, portno;
	socklen_t clilen;
	struct sockaddr_in serv_addr, cli_addr;
	
	if(argc < 2){
		fprintf(stderr, "ERROR, no port provided\n");
		exit(1);	
	}	
	portno = atoi(argv[1]);
	
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

	clilen = sizeof(cli_addr);
	while(1){
		char buffer[256];
		bzero(buffer, 256);			
		int n = recvfrom(sockfd, buffer, 255, 0, (struct sockaddr *) &cli_addr, &clilen);
		printf("Client requesting the file: %s\n", buffer);
		if(n > 0){
			dostuff(sockfd, cli_addr, buffer);
		}	
		//close(sockfd);	
	}									
}
