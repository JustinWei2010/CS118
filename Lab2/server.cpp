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
using namespace std;

void error(string msg)
{
	perror(msg.c_str());
	exit(1);
}

void sendFile(int sock, struct sockaddr_in &client, ifstream &input, long file_size)
{
	int n;
	socklen_t clilen = sizeof(client);
	char packet[8192];
	long count = 8192;
	
	while(count < file_size){
		bzero(packet, 8192);
		input.read(packet, 8192);
		n = sendto(sock, packet, 8192, 0, (struct sockaddr *) &client, clilen); 
		if(n < 0)
			printf("Error sending file to client\n");
		count += 8192;
	}

	if(file_size > 8192)
		count = 8192 - count % file_size;
	else
		count = file_size;
	bzero(packet, 8192);
	input.read(packet, count);
	n = sendto(sock, packet, count, 0, (struct sockaddr *) &client, clilen);
	if(n < 0)
		printf("Error sending file to client\n");
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
	long size = file.tellg();
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
		msg = "File not found";
		n = sendto(sock, msg.c_str(), msg.length(), 0, (struct sockaddr *) &client, clilen);
	}
	file.close();
}	

int main(int argc, char *argv[])
{
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
