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
void setPacketHeader(FilePacket *packet, unsigned long packet_num, struct sockaddr_in &client, int portno) //11/27
{
        packet->source_port = portno; //11/27
        packet->dest_port = 10000;
        packet->seq_num = packet_num;
        packet->checksum = 0;
        packet->window_size = 0;
}

void sendFile(int sock, struct sockaddr_in &client, ifstream &input, unsigned long file_size, int portno, int p_loss, int p_cor) //11/27
{
        int n;
        socklen_t clilen = sizeof(client);
        FilePacket packet;
        AckPacket ack_packet;
        long count = PAYLOAD_SIZE;
        unsigned long packet_num = 1;
        int error = 0;

        while(count < file_size){
                bzero(&packet, PACKET_SIZE);
                setPacketHeader(&packet, packet_num, client, portno);
                bzero(&ack_packet, ACK_SIZE);
                input.read(packet.payload, PAYLOAD_SIZE);
                n = sendto(sock, (char *)&packet, PACKET_SIZE, 0, (struct sockaddr *) &client, clilen);
                if(n < 0)
                        printf("Error sending file to client\n");

                while(random() % 100 < p_loss || random() % 100 < p_cor)  //simulate ack loss and corruption, (not go back n)
                {
                        sleep(TIMEOUT);
                        error = 1;
                }
                //while(random() % 100 < p_cor)
                //      sleep(TIMEOUT);
                if(error ==1)
                        continue;

                n = recvfrom(sock, (char *)&ack_packet, ACK_SIZE, 0, (struct sockaddr *) &client, &clilen);
                printf("Client acknowledged recieving packet %lu\n", ack_packet.ack_num);
                count += PAYLOAD_SIZE;
                packet_num++;
        }
        
        if(file_size > PAYLOAD_SIZE)
                count = PAYLOAD_SIZE - count % file_size;
        else
                count = file_size;
        bzero(&packet, PACKET_SIZE);
        setPacketHeader(&packet, packet_num, client, portno); //11/27
        bzero(&ack_packet, ACK_SIZE);
        input.read(packet.payload, count);
        n = sendto(sock, (char *)&packet, PACKET_SIZE, 0, (struct sockaddr *) &client, clilen);
        if(n < 0)
                printf("Error sending file to client\n");

        while(random() % 100 < p_loss || random() % 100 < p_cor) //11/27
        {
                sleep(TIMEOUT);
                 n = sendto(sock, (char *)&packet, PACKET_SIZE, 0, (struct sockaddr *) &client, clilen);
        if(n < 0)
                printf("Error sending file to client\n");
        }
        n = recvfrom(sock, (char *)&ack_packet, ACK_SIZE, 0, (struct sockaddr *) &client, &clilen);
        printf("Client acknowledged recieving packet %lu\n", ack_packet.ack_num);
}

void dostuff(int sock, struct sockaddr_in &client, char *path, int portno, int p_loss, int p_cor) //11/27
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
                sendFile(sock, client, file, size, portno, p_loss, p_cor); //11/27
        }else{
                printf("File not found\n");
                msg = "File not found";
                n = sendto(sock, msg.c_str(), msg.length(), 0, (struct sockaddr *) &client, clilen);
        }
        file.close();
}

int main(int argc, char *argv[])
{
        int sockfd, sportno, cwnd, p_cor, p_loss;
        socklen_t clilen;
        struct sockaddr_in serv_addr, cli_addr;

        if(argc < 2){
                fprintf(stderr, "ERROR, no port provided\n");
                exit(1);
        }
        sportno = atoi(argv[1]);
        cwnd = atoi(argv[2]); //11/27
        p_loss = atoi(argv[3]); //11/27
        p_cor = atoi(argv[4]); //11/27

        sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if(sockfd < 0){
                error("ERROR opening socket");
        }

        bzero((char *) &serv_addr, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(sportno); //11/27
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
                if(rand() % 100 < p_cor) { //11/27
                        printf("Corrupt Request");
                        continue;
                }
                printf("Client requesting the file: %s\n", buffer);
                if(n > 0){
                        dostuff(sockfd, cli_addr, buffer, sportno, p_loss, p_cor); //11/27
                }
                //close(sockfd);        
        }
}

