/* 				ESKADMAS AYENEW TEFERA - s236174

		TCP SEQUENTIAL SERVER THAT ITERATIVELY TRANSFERS FILES
		
	The Server that accepts file transfer request(s) from the client and sends the requested file(s) back to the client.
	The file(s) available for being sent by the server is/are the ones accessible in the server file system from the working 
	directory of the server.  
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <signal.h>
#include <sys/wait.h>

#include "../mysocket.h"
#include "../errlib.h"
#include "../sockwrap.h"

#define BUFSIZE		500
#define FILELEN		132

char *prog_name;
int serve_client(int con_req_receiver_skt);

int main(int argc, char *argv[])
{

	int			con_req_receiver_skt;	/* The Socket at which the Server accepts Client requests */
	uint16_t		lport_n, lport_h;	/* The port where the Server listens(net/host byte order response) */
	int			bklog = 10; 		/* Listening backlog */
	int			s;			/* The Socket at the server*/
	int 			result, addrlen;
	struct sockaddr_in	saddr, caddr;		/* Server and client address structures */
	short 			port;

	prog_name=argv[0];

	/* Check Arguments */
	
	if(argc != 2){
		err_msg("usage: (%s) <port>\n", prog_name);
		exit(1);
	}
	port=strtol(argv[1], NULL, 0);	

	/* Creating Socket */
	
	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(s < 0){
		err_msg("Socket() failed!\n");
		exit(1);
	}
	printf("[%s] Socket created \n", prog_name); 

	/* Bind Socket to any local IP address */
	
	saddr.sin_family=AF_INET;
	saddr.sin_port=htons(port);
	saddr.sin_addr.s_addr=INADDR_ANY;

	result=bind(s, (struct sockaddr *)&saddr, sizeof(saddr));
	if(result == -1){
		err_msg("Bind() failed! \n");
		exit(1);
	}
	
	/* Listen */
	
	result=listen(s, bklog);
	if(result == -1){
		err_msg("Listen() failed! \n");
		exit(1);
	}
	printf("[%s] Listening to client request. . .  \n", prog_name);	
	
	/* Main Server Loop */
	
	for(;;)
	{
		/* Accept Client request */
		
		addrlen = sizeof(struct sockaddr_in);               
		con_req_receiver_skt = accept(s, (struct sockaddr *) &caddr, &addrlen);

		if(con_req_receiver_skt < 0){
			err_msg("Accept() failed! \n");
			exit(1);
		}		
		/* Serve the client on socket s */

                serve_client(con_req_receiver_skt);
	}
	return 0;
}

int serve_client(int con_req_receiver_skt)
{
	char			filename[FILELEN];	/* The name of the requested file */
	char			t_buf[BUFSIZE];		/* Transmission Buffer */
	char			r_buf[BUFSIZE];		/* Reception Buffer */		
	char			file[FILELEN];		/* The contents of a file */
	int 			sent_char;		/* Length of the sent message */
	int 			read_char;		/* Length of the received message */
	struct stat 		statbuf;		/* stat syscall is used to obtain the timestamp of the last file modification of the file */
	uint32_t 		filesize;
	uint32_t 		lastmod;
	uint16_t		lport_n, lport_h;	/* The port where the server listens(net/host byte order response) */
	int 			result, addrlen, n;
	FILE			*fd;
	
        for(;;)
	{               
		read_char = recv(con_req_receiver_skt, r_buf, BUFSIZE, 0);
		if(read_char < 0){
			printf("Read error \n");
			close(con_req_receiver_skt);
			break;
		}
		else if(read_char == 0){
			printf("[%s] Connection closed by party \n", prog_name);
			close(con_req_receiver_skt);
			break;
		}
		else
		{
			if (strncmp(r_buf, "QUIT\r\n", 6) == 0)		/* Client request to quit the connection */
			{
				printf("[%s] -- QUIT command has received from Client -- \n", prog_name);
				close(con_req_receiver_skt);
				break;
			}
			else if(strncmp(r_buf, "GET ", 4)==0)		/* A command from Client to request a file */
			{
				int i;		
				for(i=0; i<strlen(r_buf)&& r_buf[i] != '\r'; i++)
					continue;
					
				i -=4;
				char *filename;
				filename = (char *) malloc(sizeof(char) * (i+1));
				strncpy(filename, &r_buf[4], i);
				filename[i] = '\0';
				
				printf("\n[%s] The requested file is: %s \n", prog_name, filename);

				fd = fopen(filename, "r");		/* Open file */
				if (fd == NULL)
				{
					printf("[%s] -- file not found! -- \n", prog_name);

					if (send(con_req_receiver_skt, "-ERR\r\n", 6, 0) != 6)
	    					printf("[%s] -- error in sending the response \n", prog_name);
					continue;
				}		
				else
				{							
					/* Get file last modification time */
					
					struct stat st;
					if (stat(filename, &st)) 
						perror(filename);
						
					else					
						printf("[%s] [%s] lastmod time is: %lld\n", prog_name, filename, (long long)st.st_mtim.tv_sec);				
					lastmod = st.st_mtim.tv_sec;
					
					printf("[%s] -- [%s] opened for reading -- \n", prog_name, filename);					
					printf("[%s] -- sending file -- \n", prog_name);

					/* Get file size */
					
					fseek (fd, 0, SEEK_END);   
					filesize=ftell(fd);
					rewind(fd);
					snprintf(t_buf, 6, "+OK\r\n");				
					uint32_t tmp = htonl(filesize);
					memcpy(&t_buf[5], &tmp, 4);
					tmp = htonl(lastmod);
					memcpy(&t_buf[9], &tmp, 4);
							
					sent_char = fread(&t_buf[13], sizeof(char), BUFSIZE-13, fd);	/* Read the file in the buffer */
					if (send(con_req_receiver_skt, t_buf, sent_char + 13, 0) != (sent_char + 13))/* The first packet */
					{
						printf("[%s]--error in sending a packet -- \n", prog_name);
						close(con_req_receiver_skt);
						break;
					}

					/* Send the rest of the file */
					
					bzero(t_buf, BUFSIZE); 
					uint32_t sentbytes = BUFSIZE - 13;  /* The file content */
					while ((sent_char = fread(t_buf, sizeof(char), BUFSIZE, fd)) > 0)	
					{
						if (send(con_req_receiver_skt, t_buf, sent_char, 0) != sent_char)
						{
							printf("[%s] -- error in sending a file -- \n", prog_name);
							close(con_req_receiver_skt);
							break;
						}
						sentbytes += sent_char;
						bzero(t_buf, BUFSIZE);
					}
					printf("[%s] -- file has sent --\n", prog_name);
					fclose(fd);
				}
			}
				/* Handle invalid command */
			else	
			{
				printf("[%s] -- invalid command: %s --\n", prog_name, r_buf);

				if (send(con_req_receiver_skt, "-ERR\r\n", 6, 0) != 6)

	    				printf("[%s] -- error in sending the message --\n", prog_name);

				printf("[%s] Closing Socket %d ...\n", prog_name, con_req_receiver_skt);

				continue;
			}
			
		}
	}
	return 0;
}
