#include <termios.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <zlib.h>
int sockfd, newsockfd;
int portno=0;
unsigned int clilen;
int portflag=0;
void error(char *msg)
{
    perror(msg);
    exit(1);
}

int pid;
/*
void signal_fun(int num){
	if(num==SIGINT)
		kill(pid, SIGINT);
	if(num==SIGPIPE){
		int the_status;
		waitpid(pid, &the_status, 0);
		//printf("mark4\n");
		fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n", WIFSIGNALED(the_status), WEXITSTATUS(the_status));
		close(sockfd);
		close(newsockfd);
		exit(0);
	}
}

*/
int main(int argc, char** argv){
/*	char tail[2];
	tail[0]='\r';
	tail[1]='\n';
	*/
	z_stream strm1;
	z_stream strm2;
	
	
	int compressflag=0;
    struct sockaddr_in serv_addr, cli_addr;

	
	struct option options[] = 
    {
      {"port", 1, NULL, 'p'},
	  {"compress",0, NULL, 'c'},
      {0, 0, 0, 0}
    };
	

	int ret;
	while((ret=getopt_long(argc, argv, "", options, NULL))!=-1){
		
		if(ret=='p') {
			portno=atoi(optarg);
			portflag=1;
		}
		else if (ret=='c')
			compressflag=1;
		
		else {
			fprintf(stderr, "cannot recognize this option\n");
			exit(1);
		}
	}
	
	if(portflag==0){
		fprintf(stderr, "there is no port number, error \n");
		exit(1);
	}
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");
   // memset((char *) &serv_addr,0, sizeof(serv_addr));
  
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);
    if (bind(sockfd, (struct sockaddr *) &serv_addr,
              sizeof(serv_addr)) < 0) 
        error("ERROR on binding");
    listen(sockfd,5);
    clilen = sizeof(cli_addr);
    newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
    if (newsockfd < 0) {
        close(sockfd);
		error("ERROR on accept");
		
	}
	//signal(SIGINT, signal_fun);
	//signal(SIGPIPE, signal_fun);
	int ttos[2];
	int stot[2];
	
	if(pipe(ttos)==-1||pipe(stot)==-1){
		close(sockfd);
		close(newsockfd);
		error("failure in making pipes");
	}
	
	pid=fork();
	if(pid<0){
		close(sockfd);
		close(newsockfd);
		close(ttos[1]);
		close(stot[0]);
	
		
		error("failure in forking");

	}
	else if (pid==0){
		//child, the shell
			close(ttos[1]);
			close(stot[0]);
			close(0);
			dup(ttos[0]);
			close(ttos[0]);
			close(1);
			dup(stot[1]);
			close(2);
			dup(stot[1]);
			
			close(stot[1]);
			
		
			char * args[2];
			char a[]="/bin/bash";
			args[0]=a;
			args[1]=NULL;
			//printf("%s\n",args[0]);
			//printf("%s\n",args[1]);
			if(execvp("/bin/bash",args)==-1){
				close(sockfd);
				close(newsockfd);
				close(ttos[1]);
				close(stot[0]);
				error("failure in excvep");
			}		
	
		
		
	}
	else {
		//parent
		struct pollfd the_pollfd[2];
			
		close(ttos[0]);
		close(stot[1]);
		the_pollfd[0].fd=newsockfd;
		the_pollfd[0].events=POLLIN|POLLHUP|POLLERR;
		the_pollfd[1].fd=stot[0];
		the_pollfd[1].events=POLLIN|POLLHUP|POLLERR;
		
		
		
		while(1){
			int ret=poll(the_pollfd,2,0);
			if(ret==-1){
					close(sockfd);
					close(newsockfd);
					error("failure in polling");
			}
			else if(the_pollfd[0].revents & POLLIN){
		 //FROM THE CLIENT
		
				if(compressflag){
					unsigned char buf[512];
					unsigned char compressbuf[512];
					int readret=read(newsockfd,buf, 512);
					if(readret==-1){
						close(sockfd);
						close(newsockfd);
						error("failure in reading");
					}
					//decompress
					memset(compressbuf, 512, readret);
					strm2.zalloc = Z_NULL;
					strm2.zfree = Z_NULL;
					strm2.opaque = Z_NULL;
					if(inflateInit(&strm2) != Z_OK){
					close(sockfd);
					close(newsockfd);
					error("failure in initializing infaltion");
					}
				
					strm2.avail_out = 512;
					strm2.next_out = compressbuf;
					strm2.avail_in = readret;
					strm2.next_in = buf;
				
					do{

						if(inflate(&strm2, Z_SYNC_FLUSH) != Z_OK){
							close(sockfd);
							close(newsockfd);
							error("failure in infalte");
						}
					}while(strm2.avail_in > 0);
				
				
				
				
				int full=512-strm2.avail_out;
				inflateEnd(&strm2);
				
				char lf='\n';
				
				for(int i=0;i<full;i++){
					if(buf[i]=='\r' ||buf[i]=='\n'){
						write(ttos[1],&lf,2);
					}
					else if(buf[i]==0x03){
								//^c
							kill(pid, SIGINT);
						}
					else if(buf[i]==0x04){
							close(ttos[1]);
							//////////////TODO
							///////////////WHAT DO WHEN EOF
							
							/*close(sockfd);
							close(newsockfd);
							close(stot);
							close(ttos[1]);
							int the_status;
							waitpid(pid, &the_status,0);
							//printf("mark1\n");
							fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n", WIFSIGNALED(the_status), WEXITSTATUS(the_status));

							exit(0);
							*/
					}
					else {
						write(ttos[1], &compressbuf[i],1);
						//printf("1\n");
						//printf("2\n");
					}
				}
					
				}
		
				else {
					unsigned char buf[512];
					int readret=read(newsockfd,buf, 512);
					if(readret==-1){
						close(sockfd);
						close(newsockfd);
						error("failure in reading");
					}
					for(int i=0;i<readret;i++){
						if(buf[i]==0x03){
								//^c
							kill(pid, SIGINT);
						}
						else if(buf[i]==0x04){
							//////////////TODO
							///////////////WHAT DO WHEN EOF
							
							
							
							close(ttos[1]);
							/*int the_status;
							waitpid(pid, &the_status,0);
							//printf("mark1\n");
							fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n", WIFSIGNALED(the_status), WEXITSTATUS(the_status));
							close(sockfd);
							close(newsockfd);
							exit(0);
							*/
						}
						else if(buf[i]=='\r' ||buf[i]=='\n'){
								char temp='\n';
								write(ttos[1],&temp,1);
						}
						else{
								write(ttos[1],&buf[i],1);
								//printf("2\n");
						}
					}	
				}		
			}
			
			else if (the_pollfd[1].revents & POLLIN){
				//income from the shell
				if(compressflag){
					//compress it
				unsigned char buf[512];
				unsigned char compressbuf[512];
					
				int readret=read(stot[0],buf,512);
					//printf("ha\n");
				if(readret==-1){
					close(sockfd);
					close(newsockfd);
					error("failure in reading");
				}
				memset(compressbuf, 0, readret);
				///TODO check the things in readret for things like EOF before compressing itoa

				
				
				strm1.zalloc = Z_NULL;
				strm1.zfree = Z_NULL;
				strm1.opaque = Z_NULL;
				if(deflateInit(&strm1, Z_DEFAULT_COMPRESSION) != Z_OK){
					close(sockfd);
					close(newsockfd);
					error("failure in initializing defaltion");
				}
				
				strm1.avail_out = 512;
				strm1.next_out = compressbuf;
				strm1.avail_in = readret;
				strm1.next_in = buf;
				
				do{

				if(deflate(&strm1, Z_SYNC_FLUSH) != Z_OK){
					close(sockfd);
					close(newsockfd);
					error("failure in deflate");
				}
				}while(strm1.avail_in > 0);
				
				
				write(newsockfd, &compressbuf, 512 - strm1.avail_out);
			
				deflateEnd(&strm1);					
				for(int i=0;i<readret;i++){
					if(buf[i]==0x04){
								close(stot[0]);
								close(ttos[1]);
								int the_status;
								close(sockfd);
								close(newsockfd);
								waitpid(pid, &the_status,0);
								//printf("mark2\n");
								fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n", WIFSIGNALED(the_status), WEXITSTATUS(the_status));

								exit(0);						
						
						
					}
				}				
				
				}
				
				else{
					//not compressed
					unsigned char buf[512];
					
					int readret=read(stot[0],buf,512);
					if(readret==-1){
							close(sockfd);
							close(newsockfd);
							error("failure in reading from the shell");
					}
	
					for(int i=0;i<readret;i++){
						if (buf[i]==0x04){
								close(ttos[1]);
								close(stot[0]);
								close(sockfd);
								close(newsockfd);
								int the_status;
								waitpid(pid, &the_status,0);
								//printf("mark2\n");
								fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n", WIFSIGNALED(the_status), WEXITSTATUS(the_status));
					
								exit(0);
						}
						else
								write(newsockfd, &buf[i],1);
							
					}			
				}
			}
			else  if (((the_pollfd[1].revents & POLLERR) == POLLERR) || ((the_pollfd[1].revents & POLLHUP) == POLLHUP)){
								close(ttos[1]);
								close(stot[0]);
								int the_status;
								waitpid(pid, &the_status,0);
								//printf("mark2\n");
								fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n", WIFSIGNALED(the_status), WEXITSTATUS(the_status));
								close(sockfd);
								close(newsockfd);
								exit(0);						
			
			}
			else  if (((the_pollfd[0].revents & POLLERR) == POLLERR) || ((the_pollfd[0].revents & POLLHUP) == POLLHUP))
				close (ttos[1]);
			continue;
		}
		
		
	}
	

								close(ttos[1]);
								close(stot[0]);
								int the_status;
								waitpid(pid, &the_status,0);
								//printf("mark2\n");
								fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n", WIFSIGNALED(the_status), WEXITSTATUS(the_status));
								close(sockfd);
								close(newsockfd);
								exit(0);						
	
	
	
	
	
	
	
}