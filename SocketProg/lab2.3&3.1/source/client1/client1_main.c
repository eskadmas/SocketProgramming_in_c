/* 				ESKADMAS AYENEW TEFERA - s236174

	The Client that requests the transfer of file(s), whose name(s) is/are specified on the
	command line as 3rd and/or subsequent parameter(s), and stores them locally in its working
	Directory. The Client prints the following line of texts to the standard output:
		- The received file name
		- The received file size (in bytes) and
		- The received file's timestamp of last modification (as a decimal number)	 
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <time.h>
#include <signal.h>

#include "../mysocket.h"
#include "../errlib.h"
#include "../sockwrap.h"

#define BUFSIZE		500

char *prog_name;

int main(int argc, char *argv[])
{

	char     	t_buf[BUFSIZE];		/* Transmission Buffer */
	char		r_buf[BUFSIZE];		/* Reception Buffer */
	int		read_char;		/* Length of the received message */
	uint32_t 	filesize;		/* The received file size in bytes */
	uint32_t 	lastmod;		/* The timestamp of the file */
	uint32_t 	tot_char_read;
	fd_set 		readset; 
	FILE 		*fd;			/* fd refers the received file that is going to be written in Client Directory */
	char 		filename[BUFSIZE];	
	short		port;
	int		s, result;		
	uint32_t 	taddr_n;
	uint16_t 	tport_n, tport_h;
	struct 		sockaddr_in saddr;	/* Server address structure */	
	struct 		in_addr sIPaddr;
	int 		len, number_of_file, remaining_file;		
	uint32_t        tmp;
	
	prog_name = argv[0];

	/* Check Arguments */
	
	if(argc < 4){
		err_msg("usage: <address> <port> <filename> [<filename>...] \n");
		exit(1);
	}
	
	result=inet_aton(argv[1], &sIPaddr);
	if(!result){
		printf("(%s) Invalid Server IP Address.\n", prog_name);
		exit(1);
	}
	
	if(sscanf(argv[2], "%" SCNu16, &tport_h)!=1) /* sscanf() reads formatted input from a string */
	{ 
		printf("Invalid port number \n");
		exit(1);
	}
	tport_n=htons(tport_h);

	/* Create Socket */
	
	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(s < 0){
		err_msg("Socket() failed.\n");
		exit(1);
	}
	else
		printf("[%s] Socket created: %d \n", prog_name, s);

	/* Specify Address to Bind */
	
	memset(&saddr, 0, sizeof(saddr));
	saddr.sin_family= AF_INET;
	saddr.sin_port= tport_n;
	saddr.sin_addr= sIPaddr;

	result = connect(s,(struct sockaddr *) &saddr, sizeof(saddr));
	if(result == -1)
	{ 
		err_msg("Connect() failed \n");
	   	close(s);
		exit(1);
	}
	else
		printf("[%s] Connection successfull \n", prog_name);

	/* Main Client Loop*/
	
	number_of_file = argc - 3;		/* arguments other than prog_name, ip, and port is/are filename(s) */
	remaining_file = number_of_file;	/* At first the number of remaining files is equal to the number of files */
	int i = 0;				/* i refers the position of the file */
	while(remaining_file > 0)
	{
		strcpy(t_buf, "GET ");
		strcat(t_buf, argv[i+3]); 
		strcat(t_buf, "\r\n");

		len = strlen(t_buf);
		if(writen(s, t_buf, len) != len){
			printf("Write error \n");
			exit(1);
		}
		
		read_char = recv(s, r_buf, BUFSIZE, 0);
		
		if (read_char < 0)
		{
			printf("[%s] recv() error\n", prog_name);
			exit(1);
		}
		
		if (strncmp(r_buf, "-ERR\r\n", 6) == 0)
		{
			printf("[%s] Error message received from server \n", prog_name);
			printf("====================================\n");
		}
		
		if (strncmp(r_buf, "+OK\r\n", 5) == 0)
		{
			memcpy(&tmp, &r_buf[5], 4);
			filesize = ntohl(tmp);
			memcpy(&tmp, &r_buf[9], 4);	
			lastmod = ntohl(tmp);
			
			strcpy(filename, argv[i+3]);
	
			fd = fopen(filename, "w");		/* The file received and to be written in the Client directory */
			if (fd == NULL)
				printf("[%s] - file %s cannot be opened \n", prog_name, filename);			
			else
			{
				if (fwrite(&r_buf[13], sizeof(char), (read_char - 13), fd) < (read_char - 13))	/* Write the first packet in the file */
					printf("[%s] - error in writing a file\n", prog_name);
					
				tot_char_read = filesize - (read_char - 13);
		
				while(tot_char_read>0)
				{
					memset(r_buf, BUFSIZE,0);
					if(tot_char_read> sizeof(r_buf))
					{
						read_char=recv(s, r_buf, sizeof(r_buf), 0);
						r_buf[sizeof(r_buf)] = '\0';
					}
					else
					{
						read_char=recv(s, r_buf, tot_char_read, 0);
						r_buf[tot_char_read]='\0';
					}
					if(read_char>0)			/* Check if the packet is received correctly */
					{
						tot_char_read -=read_char;
						fwrite(r_buf, sizeof(char), read_char, fd);
					}									   
					else
					{ 
						printf("error\n");
						break;
	  				}
				}
			}
			fclose(fd);
			
			printf("[%s] -- File[%d] information: \n", prog_name, i); 
			printf("		Received file name: %s \n", filename); 
			printf("		Received file size: %u \n", filesize);   
			printf("		Received file last modification time: %u \n", lastmod);
	
		}
		remaining_file--;
		i++;
		memset(r_buf, BUFSIZE,0);
	}
	
	if(writen(s, "QUIT\r\n", 6) != 6)
	
		printf("Write error\n");
		
	closesocket(s);
	exit(1);
}


