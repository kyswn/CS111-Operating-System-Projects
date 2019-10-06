
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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
struct termios original, afterchange;


//only use when need to restore terminal

int logflag=0;
int  logfd;
int portflag=0;
void error(char *msg)
{
    perror(msg);
	if(logflag)
		close(logfd);
	tcsetattr(0, TCSANOW, &original);							
    exit(1);
}


int main(int argc, char** argv){
	
	z_stream strm1;
	z_stream strm2;
	
	int sockfd;
	int portno=0;;
	
	int compressflag=0;
    struct sockaddr_in serv_addr;
    struct hostent *server;
	struct option options[] = 
    {
      {"port", 1, NULL, 'p'},
	  {"compress",0, NULL, 'c'},
	  {"log", 1, NULL, 'l'},
      {0, 0, 0, 0}
    };

	
	int ret;
	char* logfile;
	while((ret=getopt_long(argc, argv, "", options, NULL))!=-1){
		if(ret == 'p'){
			//printf("%s",optarg);
			portno=atoi(optarg);
			//printf("%d", portno);
			portflag=1;
		}
		else if (ret=='l'){
			logflag=1;
			logfile=optarg;
			
			
		}
		
		else if (ret=='c'){
			compressflag=1;
		}
		
		else {
			//printf("%d", ret);
			fprintf(stderr, "cannot recognoze this option\n");
			exit(1);
		}
	}
	if(logflag){
		logfd=open(logfile, O_WRONLY|O_CREAT|O_TRUNC, 0666);
			if(logfd<0){
				fprintf(stderr, "failure in creating log file\n");
				exit(1);
			}
	}

	if(portflag==0){
		fprintf(stderr,"there is no port number, error\n");
		exit(1);
	}
	char tail[2];
	tail[0]='\r';
	tail[1]='\n';
	
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        error("ERROR opening socket");
	}
	server=gethostbyname("localhost");
	if(server==NULL){		
		close(sockfd);
		error("failure in getting the server");
		
	}
	
	memset((char *) &serv_addr,0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    memcpy((char *)&serv_addr.sin_addr.s_addr, (char *)server->h_addr, 
         server->h_length);
    serv_addr.sin_port = htons(portno);
    if (connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0) {
        close(sockfd);
		error("ERROR connecting");
		
	}
	
	tcgetattr(0,&original);
	tcgetattr(0,&afterchange);
	afterchange.c_iflag = ISTRIP;	/* only lower 7 bits	*/
	afterchange.c_oflag = 0;		/* no processing	*/
	afterchange.c_lflag = 0;		/* no processing	*/
	
	if(tcsetattr(0, TCSANOW, &afterchange)==-1){
		fprintf(stderr, "failure in setting the attributes\n");
		exit(1);
	}
	
	
	if(compressflag==0){
	struct pollfd the_pollfd[2];
	the_pollfd[0].fd=0;
	the_pollfd[0].events=POLLIN|POLLHUP|POLLERR;
	the_pollfd[1].fd=sockfd;
	the_pollfd[1].events=POLLIN|POLLHUP|POLLERR;
	while(1){
		int ret=poll(the_pollfd,2,0);
		if(ret==-1){
			close(sockfd);
			error("failure in polling");
			
		}
		if(the_pollfd[0].revents & POLLIN){
					//keyboard has input
			char buf[512];
					
			int readret=read(0,buf,512);
					//printf("ha\n");
			if(readret==-1){
				close(sockfd);
				error("failure in reading");
			}
			for(int i=0;i<readret;i++){
				if(buf[i]=='\r' ||buf[i]=='\n'){
						write(1,tail,2);
						write(sockfd,&buf[i],1);
				}
				else {
					write(1, &buf[i],1);
					//printf("1\n");
					write(sockfd,&buf[i],1);
					//printf("2\n");
				}
			}
			if(logflag){
			char beginpart[14] = "SENT x bytes: ";
		  beginpart[5] = '0' + readret;
		  write(logfd, beginpart, 14);
		  write(logfd, buf, readret);
		  write(logfd, "\n", 1);
			}
		}
		
		if (the_pollfd[1].revents & POLLIN){
			//server to client has input 
			char buf[512];
					
			int readret=read(sockfd,buf,512);
			if(readret==-1){
				close(sockfd);
				error("failure in reading");
			}
			if(readret==0){
				close(sockfd);
				if(logflag)
					close(logfd);
				tcsetattr(0, TCSANOW, &original);
				exit(0);
			}
			for(int i=0;i<readret;i++){
				if(buf[i]==0x0A || buf[i]==0x0D)
					write(1,tail,2);
				else
					write(1, &buf[i],1);
			}
			
			if(logflag){
		      char beginpart[18] = "RECEIVED x bytes: ";
				beginpart[9] = '0' + readret;
			write(logfd, beginpart, 18);
	      write(logfd, buf, readret);
	      write(logfd, "\n", 1);
			}
			
		}	
		if ( the_pollfd[1].revents & (POLLHUP &POLLERR))  {
			//POLLHUP or POLLERR
			close(sockfd);
			tcsetattr(0, TCSANOW, &original);	
			exit(0);
					
		}
		continue;
	}	
	}
	else {
		//compressed
		struct pollfd the_pollfd[2];
		the_pollfd[0].fd=0;
		the_pollfd[0].events=POLLIN|POLLHUP|POLLERR;
		the_pollfd[1].fd=sockfd;
		the_pollfd[1].events=POLLIN|POLLHUP|POLLERR;
		while(1){
			int ret=poll(the_pollfd,2,0);
			if(ret==-1){
				close(sockfd);
				error("failure in polling");
			}
			else if(the_pollfd[0].revents & POLLIN){
					//keyboard
				unsigned char buf[512];
				unsigned char compressbuf[512];
					
				int readret=read(0,buf,512);
					//printf("ha\n");
				
				if(readret==-1){
					close(sockfd);
					error("failure in reading");
				}
				
					
				memset(compressbuf, 0, 512);
				strm1.zalloc = Z_NULL;
				strm1.zfree = Z_NULL;
				strm1.opaque = Z_NULL;
				if(deflateInit(&strm1,Z_DEFAULT_COMPRESSION) != Z_OK){
					close(sockfd);
					error("failure in initializing defaltion");
				}
				
				strm1.avail_out = 512;
				strm1.next_out = compressbuf;
				strm1.avail_in = readret;
				strm1.next_in = buf;
				
				do{

				if(deflate(&strm1, Z_SYNC_FLUSH) != Z_OK){
					close(sockfd);
					error("failure in deflate");
				}
				}while(strm1.avail_in > 0);
				
				write(sockfd, &compressbuf, 512 - strm1.avail_out);
				if(logflag){
		  char beginpart[14] = "SENT x bytes: ";
		  beginpart[5] = '0' + 512 - strm1.avail_out;
		  write(logfd, beginpart, 14);
		  write(logfd, compressbuf, 512 - strm1.avail_out);
		  write(logfd, "\n", 1);
				}
				
				deflateEnd(&strm1);
				
				
				
				
				
				for(int i=0;i<readret;i++){
					if(buf[i]=='\r' ||buf[i]=='\n'){
						write(1,tail,2);
					}
					else {
						write(1, &buf[i],1);
						//printf("1\n");
						//printf("2\n");
					}
				}
				
			}				
			
			else if(the_pollfd[1].revents & POLLIN){
				//read from socket
				unsigned char buf[512];
				unsigned char compressbuf[512];
					
				int readret=read(sockfd,buf,512);
				if(readret==-1){
					close(sockfd);
					error("failure in reading");
				}
				
				if(readret==0){
					close(sockfd);
					if(logflag)
						close(logfd);
					tcsetattr(0, TCSANOW, &original);
					exit(0);
				}				
				memset(compressbuf, 0, 512);
				if(logflag){
	      char beginpart[18] = "RECEIVED x bytes: ";
	      beginpart[9] = '0' + readret;
	      write(logfd, beginpart, 18);
	      write(logfd, buf, readret);
	      write(logfd, "\n", 1);
				}
				//start to decompress
				strm2.zalloc = Z_NULL;
				strm2.zfree = Z_NULL;
				strm2.opaque = Z_NULL;
				if(inflateInit(&strm2) != Z_OK){
					close(sockfd);
					error("failure in initializing infaltion");
				}
				
				strm2.avail_out = 512;
				strm2.next_out = compressbuf;
				strm2.avail_in = readret;
				strm2.next_in = buf;
				
				do{

				if(inflate(&strm2, Z_SYNC_FLUSH) != Z_OK){
					close(sockfd);
					error("failure in infalte");
				}
				}while(strm2.avail_in > 0);
				
				
				inflateEnd(&strm2);
				
				int full=512-strm2.avail_out;
				
				
				
				
				for(int i=0;i<full;i++){
					if(buf[i]=='\r' ||buf[i]=='\n'){
						write(1,tail,2);
					}
					else {
						write(1, &compressbuf[i],1);
						//printf("1\n");
						//printf("2\n");
					}
				}
				
			}
			
			
			
			
			
			
			//else if ( the_pollfd[1].revents & (POLLHUP &POLLERR))  {
			else if (((the_pollfd[1].revents & POLLERR) == POLLERR) || ((the_pollfd[1].revents & POLLHUP) == POLLHUP)){
			//POLLHUP or POLLERR
				close(sockfd);
				tcsetattr(0, TCSANOW, &original);	
				exit(0);
					
			}

		}
	}
	
	
		
		
					
	tcsetattr(0, TCSANOW, &original);			
	exit(0);
}
	