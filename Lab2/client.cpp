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
#include <iostream>
using namespace std;

void error(string msg)
{
	perror(msg.c_str());
	exit(1);	
}

int main(int argc, char *argv[])
{
	int sockfd, portno, n;
	socklen_t serv_len;
	struct sockaddr_in serv_addr, cli_addr;
	struct hostent *server;
	string file_name;

	if(argc < 4){
		fprintf(stderr, "ERROR need hostname, port number, and filename in that order\n");	
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

	bzero((char *) &serv_addr, sizeof(serv_addr));		
	serv_addr.sin_family = AF_INET;
	bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
	serv_addr.sin_port = htons(portno);	

	serv_len = sizeof(serv_addr);
	
	n = sendto(sockfd, file_name.c_str(), file_name.length(), 0, (struct sockaddr *) &serv_addr, serv_len);
	if(n < 0)
		error("Error sending file name");	

	char buffer[256];
	bzero(buffer, 256);
	n = recvfrom(sockfd, buffer, 255, 0, (struct sockaddr *) &serv_addr, &serv_len);
	string msg(buffer);
	if(msg == "File not found"){
		printf("File does not exist on server\n");
		exit(0);
	}

	cout << "Retrieving file(" << buffer << "bytes)..." << endl;

	int file_size = atoi(buffer);
	
	file_name = "/home/cs118/client/" + file_name.substr(file_name.find_last_of("/")+1);
	ofstream output;
	output.open(file_name.c_str()); 
	if(!output.is_open())
		error("Error creating new file");
	
	bzero(buffer, 256);
	n =	recvfrom(sockfd, buffer, file_size, 0, (struct sockaddr *) &serv_addr, &serv_len); 
	if(n > 0)
		output.write(buffer, file_size);		

	output.close();	
}
