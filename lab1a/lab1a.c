//NAME: Yining Wang
//ID: 504983099
//EMAIL: wangyining@g.ucla.edu 
 
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

struct termios original, afterchange;
int pid;
void signalfun(int num){
	if (num==SIGPIPE){
		int the_status;
		waitpid(pid, &the_status, 0);
		//printf("mark4\n");
		fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n", WIFSIGNALED(the_status), WEXITSTATUS(the_status));
		tcsetattr(0, TCSANOW, &original);
		exit(0);
	}
}

int main(int argc, char** argv){
	struct option options[] = 
    {
      {"shell", 0, NULL, 's'},
      {0, 0, 0, 0}
    };
	
	int flag=0;
	
	int ret;
	while((ret=getopt_long(argc, argv, "", options, NULL))!=-1){
		
		if(ret=='s') 
			flag=1;
		else{
			fprintf(stderr, "cannot recognoze this option\n");
			exit(1);
		}
	}
	
	//set up the terminal
	tcgetattr(0,&original);
	tcgetattr(0,&afterchange);
	afterchange.c_iflag = ISTRIP;	/* only lower 7 bits	*/
	afterchange.c_oflag = 0;		/* no processing	*/
	afterchange.c_lflag = 0;		/* no processing	*/
	
	if(tcsetattr(0, TCSANOW, &afterchange)==-1){
		fprintf(stderr, "failure in setting the attributes\n");
		exit(1);
	}
	
	char tail[2];
	tail[0]='\r';
	tail[1]='\n';
	
	if(flag==0){
		//without shell option
		char buf[256];
		while(1){
			int readret=read(0,buf,256);
			if(readret==-1){
				fprintf(stderr, "failure in reading\n");
				tcsetattr(0, TCSANOW, &original);
				exit(1);
			}
	
			for(int i=0;i<readret;i++){
				if(buf[i]==0x04){
					tcsetattr(0, TCSANOW, &original);
					exit(0);
				}
				else if(buf[i]=='\r' ||buf[i]=='\n'){
					write(1,tail,2);
				}
				else
					write(1, &buf[i],1);
			}
		
		}	
		//tcsetattr(0, TCSANOW, &original);
		//exit(0);
	}
	else {
		//with shell option
		
		int ttos[2];
		int stot[2];
		if (pipe(ttos)==-1 || pipe(stot)==-1){
			fprintf(stderr, "error in creating pipe \n");
			tcsetattr(0, TCSANOW, &original);
			exit(1);
		}
		
		
		pid=fork();
		if(pid==0){
		//child	
			//printf("child\n");
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
				fprintf(stderr, "failure in execvpt\n");
				tcsetattr(0, TCSANOW, &original);
				exit(1);
			}
		}
		
		else if(pid>0){
		//parent
			struct pollfd the_pollfd[2];
			signal(SIGPIPE, signalfun);
			close(ttos[0]);
			close(stot[1]);
			the_pollfd[0].fd=0;
			the_pollfd[0].events=POLLIN|POLLHUP|POLLERR;
			the_pollfd[1].fd=stot[0];
			the_pollfd[1].events=POLLIN|POLLHUP|POLLERR;
			while(1){
				int ret=poll(the_pollfd,2,0);
				if(ret==-1){
					fprintf(stderr, "failure in polling\n");
					tcsetattr(0, TCSANOW, &original);
					exit(1);
				}
				else if(the_pollfd[0].revents & POLLIN){
					//keyboard
					char buf[256];
					
					int readret=read(0,buf,256);
					//printf("ha\n");
					if(readret==-1){
						fprintf(stderr, "failure in reading\n");
						tcsetattr(0, TCSANOW, &original);
						exit(1);
						}
	
					for(int i=0;i<readret;i++){
						if(buf[i]==0x03){
								//^c
							kill(pid, SIGINT);
						}
						else if(buf[i]==0x04){
							close(ttos[1]);
							int the_status;
							waitpid(pid, &the_status,0);
							//printf("mark1\n");
							fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n", WIFSIGNALED(the_status), WEXITSTATUS(the_status));
							tcsetattr(0, TCSANOW, &original);
							exit(0);
						}
						else if(buf[i]=='\r' ||buf[i]=='\n'){
								write(1,tail,2);
								char temp='\n';
								write(ttos[1],&temp,1);
						}
						else{
								write(1, &buf[i],1);
								//printf("1\n");
								write(ttos[1],&buf[i],1);
								//printf("2\n");
						}
							
					}
				}
			    else if (the_pollfd[1].revents & POLLIN){
					//shell to terminal
					char buf[256];
					
					int readret=read(stot[0],buf,256);
					if(readret==-1){
							fprintf(stderr, "failure in reading\n");
							tcsetattr(0, TCSANOW, &original);
							exit(1);
						}
	
					for(int i=0;i<readret;i++){
						if(buf[i]=='\n'){
								write(1,tail,2);
							
						}
						else if (buf[i]==0x04){
								close(ttos[1]);
								int the_status;
								waitpid(pid, &the_status,0);
								//printf("mark2\n");
								fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n", WIFSIGNALED(the_status), WEXITSTATUS(the_status));
								tcsetattr(0, TCSANOW, &original);
								exit(0);
						}
						else
								write(1, &buf[i],1);
							
						}	
					
				
				}		
				else if (( the_pollfd[1].revents & (POLLHUP &POLLERR)  )   || ( the_pollfd[0].revents & (POLLHUP&POLLERR)  ) ){
					//POLLHUP or POLLERR
					int the_status;
					waitpid(pid, &the_status,0);
					//printf("mark3\n");
					fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n", WIFSIGNALED(the_status), WEXITSTATUS(the_status));
					tcsetattr(0, TCSANOW, &original);
					exit(1);
					
				}
				continue;
					
			
			}
		}
		else {
			fprintf(stderr, "failure in forking\n");
			tcsetattr(0, TCSANOW, &original);
			exit(1);
		}
		
		tcsetattr(0, TCSANOW, &original);
		exit(0);
	}
	
}