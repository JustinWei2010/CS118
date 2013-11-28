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
#include "packet.h"
using namespace std;

void error(string msg)
{
        perror(msg.c_str());
        exit(1);
}

void setAckHeader(AckPacket *packet, unsigned long packet_num, struct sockaddr_in &server)
{
        packet->source_port = 10000;
        packet->dest_port = 10000;
        packet->ack_num = packet_num;
        packet->checksum = 0;
}

void getFile(int sock, struct sockaddr_in &server, ofstream &output, unsigned long file_size, int p_loss, int p_cor) //11/27
{
        int n;
        socklen_t servlen = sizeof(server);
        FilePacket packet;
        AckPacket ack_packet;
        long count = PAYLOAD_SIZE;
        int error;
        while(count < file_size){
                error = 0;
                bzero(&packet, PACKET_SIZE);
                bzero(&ack_packet, ACK_SIZE);
                while(random() % 100 < p_loss || random() % 100 < p_cor) //11/27
                {
                        sleep(TIMEOUT);
                        error = 1;
                }
                if(error==1)
                        continue;

                n = recvfrom(sock, (char *) &packet, PACKET_SIZE, 0, (struct sockaddr *) &server, &servlen);
                //need to keep track of sequence numbers and frames
                if(n < 0){
                        printf("Error recieving file from server\n");
                }else{
                        setAckHeader(&ack_packet, packet.seq_num, server);
                        n = sendto(sock, (char *) &ack_packet, ACK_SIZE, 0, (struct sockaddr *) &server, servlen);
                        printf("Sent acknowledgement of packet %lu to server\n", packet.seq_num);
                        output.write(packet.payload, PAYLOAD_SIZE);
                }
                count += PAYLOAD_SIZE;
        }
 //Add in extra bits of file
        if(file_size > PAYLOAD_SIZE)
                count = PAYLOAD_SIZE - count % file_size;
        else
                count = file_size;
        bzero(&packet, PACKET_SIZE);
        bzero(&ack_packet, ACK_SIZE);

        while(random() % 100 < p_loss || random() % 100 < p_cor) //11/27
        {
                sleep(TIMEOUT);
        n = recvfrom(sock, (char *) &packet, PACKET_SIZE, 0, (struct sockaddr *) &server, &servlen);
        }
        if(n < 0){
                printf("Error recieving file from server\n");
        }else{
                setAckHeader(&ack_packet, packet.seq_num, server);
                n = sendto(sock, (char *) &ack_packet, ACK_SIZE, 0, (struct sockaddr *) &server, servlen);
                printf("Sent acknowledgement of packet %lu to server\n", packet.seq_num);
                output.write(packet.payload, count);
        }
}

int main(int argc, char *argv[])
{
        int sockfd, portno, n, p_loss, p_cor;
        socklen_t serv_len;
        struct sockaddr_in serv_addr, cli_addr;
        struct hostent *server;
        string file_name;

        if(argc < 6){ //11/27
                fprintf(stderr, "ERROR need hostname, port number, filename, loss prob, and corr prob in that order\n");
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
        p_loss = atoi(argv[4]); //11/27
        p_cor = atoi(argv[5]); //11/27

        bzero((char *) &serv_addr, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
        serv_addr.sin_port = htons(portno);

        serv_len = sizeof(serv_addr);
 //Request file from server        
        n = sendto(sockfd, file_name.c_str(), file_name.length(), 0, (struct sockaddr *) &serv_addr, serv_len);
        if(n < 0)
                error("Error sending file name");

        //Retrieve response from the server
        char buffer[256];
        bzero(buffer, 256);
        n = recvfrom(sockfd, buffer, 255, 0, (struct sockaddr *) &serv_addr, &serv_len);
        string msg(buffer);
        if(msg == "File not found"){
                printf("File not found on server\n");
                exit(0);
        }
 //Get file from server given the size of the file
        unsigned long file_size = strtoul(buffer, NULL, 0);
        printf("Retrieving file(%lubytes)...\n", file_size);

        file_name = "/home/cs118/Lab2/" + file_name.substr(file_name.find_last_of("/")+1); //11/27
        ofstream file;
        file.open(file_name.c_str());
        if(file.is_open())
                getFile(sockfd, serv_addr, file, file_size, p_loss, p_cor); //11/27
        else
                error("Error creating new file");
        file.close();
}