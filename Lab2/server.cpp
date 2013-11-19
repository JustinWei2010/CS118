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
	struct sockaddr_in serv_addr;
	
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

	

}
