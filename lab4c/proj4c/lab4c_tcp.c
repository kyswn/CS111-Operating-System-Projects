#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <mraa/aio.h>
#include <mraa/gpio.h>
#include <unistd.h>
#include <sys/types.h>
#include <math.h>
#include <sys/socket.h>
#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <poll.h>
#include <unistd.h>
#include <signal.h>

char* id;
char* host;
int port;
int sockfd;

sig_atomic_t volatile runflag = 1;

int stopflag=0;

int period=1;
char scale='F';
int logfileflag=0;
mraa_gpio_context button;
mraa_aio_context sensor;
int value;
FILE* fd;

void shut(){
	runflag=0;
	time_t now = time(0);
	char thetime[9];
	strftime(thetime, sizeof(thetime), "%T",localtime(&now));
	//printf("%s SHUTDOWN\n", thetime);
    dprintf(sockfd, "%s SHUTDOWN\n", thetime);
	if(logfileflag==1){
		fprintf(fd, "SHUTDOWN\n");
		fflush(fd);
	}
	
}

float changeunit(float c) {
  return c * 9.0 / 5.0 + 32.0;
}




int main(int argc, char** argv){
	
	int ret;
	struct option options[] = 
    {
      {"period", 1, NULL, 'p'},
      {"scale", 1, NULL, 's'},
      {"log", 1, NULL, 'l'},
	  {"id", 1, NULL, 'i'},
	  {"host", 1, NULL, 'h'},
      {0, 0, 0, 0}
    };
	
	while((ret=getopt_long(argc, argv, "", options, NULL))!=-1){
		
		switch (ret) {
			case 'i':
			if(strlen(optarg)!=9){
				fprintf(stderr, "the id is not 9 digits\n");
				exit(1);
			}
			id=optarg;
			
			break;
			
			case 'h':
			host=optarg;
			break;
			
			
			case 'p':
			if(atoi(optarg)<0){
				fprintf(stderr, "period is negative, error \n");
				exit(1);
			}
			else
				period=atoi(optarg);
			break;
			case 's':
			if (strlen(optarg)!=1) {
				fprintf(stderr, "the scale is not a chracter\n");
				exit(1);
			}
			if (optarg[0]!='F' && optarg[0]!= 'C'){
				fprintf(stderr, "the scale is not a recognizable scale\n");
				exit(1);
			}
			scale=optarg[0];
			break;
			case 'l':;
			
			fd=fopen(optarg, "w");
			//fd = open(optarg, O_CREAT | O_WRONLY | O_TRUNC, 0666);
			//if (fd == -1){
				//fprintf(stderr, "cannot open the logfile\n");
				//exit(1);
			//}
			logfileflag=1;
			break;
			
			default:
			fprintf(stderr, "this is not a legit option\n");
			exit(1);
		}
		
	}
	
	port=atoi(argv[argc-1]);
	if(port==0){
		fprintf(stderr, "the port number is 0\n");
		exit(1);
	}
	
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
		fprintf(stderr, "failure in building connection\n");
		exit(1);
	}
	
	struct sockaddr_in server;
	memset((char*) &server,0, sizeof(server));
	struct hostent *server_host = gethostbyname(host);
	if(server_host == NULL){
		fprintf(stderr, "failure in finding the host \n");
	}

	memcpy((char *) &server.sin_addr.s_addr,(char*) server_host->h_addr,
	 server_host->h_length);

	server.sin_family = AF_INET;
	server.sin_port = htons(port);

	if (connect(sockfd , (struct sockaddr *)&server , sizeof(server)) < 0){
		fprintf(stderr, "failure in connetcting\n");
		exit(1);
	}

	dprintf(sockfd, "ID=%s\n", id);
	fprintf(fd, "ID=%s\n", id);
	fflush(fd);
  
	
	
	
	
	
	button = mraa_gpio_init(60);
	sensor = mraa_aio_init(1);
	mraa_gpio_dir(button, MRAA_GPIO_IN);
	mraa_gpio_isr(button, MRAA_GPIO_EDGE_RISING, shut, NULL);
	 /*struct pollfd {
               int   fd;         file descriptor 
               short events;      requested events 
               short revents;     returned events 
           }
		   */
	struct pollfd the_pollfd={sockfd, POLLIN, 0};
	char* buffer=malloc(sizeof(char)* 2000);
	int index=0;
	int wordhead=0;
	while(runflag==1){
		
		//get temperature
		
		value = mraa_aio_read(sensor);
		float R = 1023.0 / value - 1.0;
		R = 100000 * R;
		float temperature = 1.0 /(log(R / 100000) / 4275 + 1.0 / 298.15) - 273.15; 
		int pollflag=1;
		time_t start = time(0);
  
		
		while(pollflag){
			poll(&the_pollfd, 1, 0);
			if (the_pollfd.revents & POLLIN) {
				char singlebuffer;
				if (read(sockfd, &singlebuffer, 1)) {
					buffer[index]=singlebuffer;
					index++;
					
					if(singlebuffer=='\n'){
						buffer[index]='\0';
						index++;
						char* word=buffer+wordhead;
						//comparing the words we get to specs's requirement words
						if(logfileflag==1){
							fprintf(fd, "%s", word); 
				
						}
						if(! strcmp(word, "OFF\n")){
							shut();
						}
							
						else if(! strcmp(word, "SCALE=F\n"))
							scale='F';
						else if (! strcmp(word, "SCALE=C\n"))
							scale='C';
						else if (! strcmp(word, "STOP\n"))
							stopflag=1;
						else if (! strcmp(word, "START\n"))
							stopflag=0;
						else if (word[0]=='P' && word[1]=='E' &&word[2]=='R' && word[3]=='I' && word[4]== 'O' && word[5]== 'D'){
							int periodtemp=atoi(word+7);
							if(periodtemp<=0){
								fprintf(stderr, "the period give at input is negative\n");
								exit(1);
							}
							else period=periodtemp;
						}
						else if (word[0]=='L' && word[1]=='O' && word[2]=='G')
							;
						else {
							fprintf(stderr, "input is invald\n");
							exit(1);
						}
						
						wordhead=index;
					}
				}
		
			}
			if(time(0)-start >=period && stopflag==0)
				pollflag=0;
			if (runflag == 0) {
				mraa_aio_close(sensor);
				mraa_gpio_close(button);
				exit(0);
			}
		
		}
		time_t now = time(0);
		/*char* thetime=malloc(sizeof(char)*10);
		thetime[9]='\0';*/
		char thetime[9];
		strftime(thetime, sizeof(thetime), "%T",localtime(&now));
		
		if(scale=='F')
			temperature=changeunit(temperature);
		
		//printf("%s %.1f\n", thetime, temperature);
        //fflush(stdout);
		
		char temp[40];
		int j=0;
		for (j=0;j<40;j++)
			temp[j]=0;
		sprintf(temp,"%s %.1f", thetime, temperature);
		dprintf(sockfd,"%s\n", temp);
		if(logfileflag==1){
			fprintf(fd, "%s\n", temp);
			fflush(fd); 
		}
	
	
	}
	
	
	
	mraa_aio_close(sensor);
	mraa_gpio_close(button);
	return 0;

	
	
}