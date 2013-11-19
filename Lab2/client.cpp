#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <strings.h>
#include <string>
using namespace std;

void error(string msg)
{
	perror(msg.c_str());
	exit(1);	
}

int main(int argc, char *argv[])
{
	int sockfd, portno, n;
	struct sockaddr_in serv_addr, cli_addr;
	struct hostent *server;
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

	bzero((char *) &serv_addr, sizeof(serv_addr));		
	serv_addr.sin_family = AF_INET;
	bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
	serv_addr.sin_port = htons(portno);	
		
	
}
