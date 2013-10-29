
#include <stdio.h>
#include <sys/types.h>   // definitions of a number of data types used in socket.h and netinet/in.h
#include <sys/socket.h>  // definitions of structures needed for sockets, e.g. sockaddr
#include <netinet/in.h>  // constants and structures needed for internet domain addresses, e.g. sockaddr_in
#include <stdlib.h>
#include <strings.h>
#include <sys/wait.h>	/* for the waitpid() system call */
#include <signal.h>	/* signal name macros, and the kill() prototype */
#include <string.h>
#include <ctime>
#include <sys/stat.h>
#include <unistd.h>
#include <iostream>
#include <sstream>
#include <fstream>
using namespace std;

void sigchld_handler(int s)
{
    while(waitpid(-1, NULL, WNOHANG) > 0);
}

void dostuff(int); /* function prototype */
void error(const char *msg)
{
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[])
{
     int sockfd, newsockfd, portno, pid;
     socklen_t clilen;
     struct sockaddr_in serv_addr, cli_addr;
     struct sigaction sa;          // for signal SIGCHLD

     if (argc < 2) {
         fprintf(stderr,"ERROR, no port provided\n");
         exit(1);
     }
     sockfd = socket(AF_INET, SOCK_STREAM, 0);
     if (sockfd < 0) 
        error("ERROR opening socket");
     bzero((char *) &serv_addr, sizeof(serv_addr));
     portno = atoi(argv[1]);
     serv_addr.sin_family = AF_INET;
     serv_addr.sin_addr.s_addr = INADDR_ANY;
     serv_addr.sin_port = htons(portno);
     
     if (bind(sockfd, (struct sockaddr *) &serv_addr,
              sizeof(serv_addr)) < 0) 
              error("ERROR on binding");
     
     listen(sockfd,5);
     
     clilen = sizeof(cli_addr);
     
     /****** Kill Zombie Processes ******/
     sa.sa_handler = sigchld_handler; // reap all dead processes
     sigemptyset(&sa.sa_mask);
     sa.sa_flags = SA_RESTART;
     if (sigaction(SIGCHLD, &sa, NULL) == -1) {
         perror("sigaction");
         exit(1);
     }
     /*********************************/
     
     while (1) {
         newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
         
         if (newsockfd < 0) 
             error("ERROR on accept");
         
         pid = fork(); //create a new process
         if (pid < 0)
             error("ERROR on fork");
         
         if (pid == 0)  { // fork() returns a value of 0 to the child process
             close(sockfd);
             dostuff(newsockfd);
             exit(0);
         }
         else //returns the process ID of the child process to the parent
             close(newsockfd); // parent doesn't need this 
     } /* end of while */
     return 0; /* we never get here */
}

/*
Get content type of file by parsing the path name 
*/
string contentType(string path)
{
	string file_name;
	file_name = path.substr(path.find_last_of("/")+1);

	string type = file_name.substr(file_name.find(".")+1);
	
	if(type == "html" || type == "htm"){
		return "text/html";		
	}else if(type == "gif"){
		return "image/gif";	
	}else if(type == "jpg" || type == "jpeg"){
		return "image/jpeg";
	}

	//******Need to handle case where file type isnt defined
	return "";
}

/*
Parses the http request sent by the client
*/
void requestParser(char *request, int sock)
{
	//Parsing the first line of the request
	int n;
	string method, path, version, delim;
	delim = " \n\r\t";
	method = strtok(request, delim.c_str());
	path = strtok(NULL, delim.c_str());
	version = strtok(NULL, delim.c_str());

	//********Need to check validity of the request message
	
	ifstream file(path.c_str());
	string status, status_msg;
	int status_num; //number to keep track of status

	//Checking status messages
	string version_num = version.substr(version.find_last_of("/")+1);
	if(version_num != "1.1" && version_num != "1.0"){
		status_msg = "505 HTTP Version Not Supported";
		status_num = 3;
	}else if(method != "GET"){
		status_msg = "405 Method Not Allowed";
		status_num = 2;
	}else if(file.is_open()){
		status_msg = "200 OK";
		status_num = 0;
	}else{
		status_msg = "404 Not Found";
		status_num = 1;
	}

	//Header messages	
	status = version + " " + status_msg + "\r\n"; 
	if(status_num == 0)
	n = write(sock, status.c_str(), status.size());

	string connection = "Connection: close\r\n";
	if(status_num == 0)
	n = write(sock, connection.c_str(), connection.size());

	//Last date
	time_t timer;
	struct tm *td;
	char date[80];

	time(&timer);
	td = gmtime(&timer);
	strftime(date, 80, "Date: %a, %d %b %Y %X GMT\r\n", td); 
	if(status_num == 0)
	n = write(sock, date, 80);

	//Server
	string server = "Server: Local Webserver\r\n";
	if(status_num == 0)
	n = write(sock, server.c_str(), server.size());	

	//Modified time
	char last_modified[80];
	struct stat attrib;
	stat(path.c_str(), &attrib);
	td = gmtime(&(attrib.st_mtime));
	strftime(last_modified, 80, "Last-Modified: %a, %d %b %Y %X GMT\r\n", td);
	if(status_num == 0)
	n = write(sock, last_modified, 80);	

	//Getting content length
	string content_length;	
	file.seekg(0, ifstream::end);
	stringstream ss;
	ss << file.tellg();
	content_length = "Content-Length: " + ss.str() + "\r\n";
	if(status_num == 0)
	n = write(sock, content_length.c_str(), content_length.size()); 

	//Getting content type
	string content_type;	
	content_type = "Content-Type: " + contentType(path) + "\r\n";
	if(status_num == 0)
	n = write(sock, content_type.c_str(), content_type.size());

	cout << status << connection << date << server << last_modified << content_length << content_type << endl;

	//Reset file pos, and also add in a CRLF before the body content	
	file.clear();
	file.seekg(0, ifstream::beg);
	if(status_num == 0)
	n = write(sock, "\n", 1);

	//Body content
	string line;
	if(file.is_open()){
		while(getline(file, line)){
			cout << line << endl;
			line += '\n';
			n = write(sock, line.c_str(), line.size());
		}
		file.close();
	}
	else{
		n = write(sock, status_msg.c_str(), status_msg.size());
	}
	return;
}

/******** DOSTUFF() *********************
 There is a separate instance of this function 
 for each connection.  It handles all communication
 once a connnection has been established.
 *****************************************/
void dostuff (int sock)
{
   int n;
   char buffer[256];
   bzero(buffer,256);
   n = read(sock,buffer,255);
   if (n < 0) error("ERROR reading from socket");
   //printf("Here is the message: %s\n",buffer);
 //  n = write(sock,"I got your message",18);
	//string request(buffer);
	requestParser(buffer, sock);
   if (n < 0) error("ERROR writing to socket");
}
