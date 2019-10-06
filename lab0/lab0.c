/*
NAME: Yining Wang
EMAIL: wangyining@g.ucla.edu
ID: 504983099

*/

#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h> 
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <fcntl.h>

void makesegmentationfault(){
	char *a=NULL;
	*a='c';
}
	
//typedef void (*signal_handler)(int);

void signal_handler_fun() {
   fprintf(stderr,"I caught a segmentation fault! \n");
	exit(4);
}

int main(int argc, char *argv[])
{
  struct option options[]=
    {
     {"input", 1, NULL, 'i'},
     {"output", 1, NULL,'o'},
     {"segfault", 0, NULL, 's'},
     {"catch", 0, NULL, 'c'},
     {"dump-core", 0, NULL, 'd'},
	    {0, 0, 0, 0}
    };

  int res=0;
  int  flagofsegmen=0;
  int  flagofcatch=0;
  int  flagofdump=0;
 
  
  while((res=getopt_long(argc, argv, "", options, NULL))!=-1){
	  switch(res){
		  case 'i': ;
		  int ifd = open(optarg, O_RDONLY);
		  if (ifd >= 0) {
			close(0);
			dup(ifd);
			close(ifd);
			}
			else{
				fprintf(stderr, "error caused by --input in opening the file %s , and the reason is %s \n", optarg, strerror(errno));
				exit(1);
				}
				break;
      case 'o':;
	int ofd = creat(optarg, 0666);
	if (ofd >= 0) {
	  close(1);
	  dup(ofd);
	  close(ofd);
	}
	else{
            fprintf(stderr, "error caused by --input in opening the file %s , and the reason is %s \n", optarg, strerror(errno));
            exit(1);
          }
		  break;
		  
    
	case 's':
	flagofsegmen=1;
	if(flagofcatch==1)
		flagofsegmen=2;
	
	break;
	
	case 'c':

	
	flagofcatch=1;
	
	break;
	  
	case 'd':
	
		flagofdump=1;
		break;

	default:
	fprintf(stderr, "there is unrecognized argument, and the correct arguments are: --input:filename --output:filename --segfault and --dump-core \n");
	exit(3);
	}
  }
  
  if(flagofsegmen==2){
		if(flagofdump==1){
			makesegmentationfault();
		}
		else{
			signal(	  SIGSEGV, signal_handler_fun);
			makesegmentationfault();
		}
	}
	if(flagofsegmen==1){
		makesegmentationfault();
	}
	
	if(flagofsegmen==0){
		  char buf;
		  int re=0;
		  while((re=read(0,&buf,sizeof(char)))>0){
		      int re2=write(1,&buf,sizeof(char));
			  if(re2<0){
				  fprintf(stderr, "has problem writing things to output");
				  exit (3);
			  }
		  }
		  if(re<0){
			 fprintf(stderr, "has problem reading things from input ");
			 exit(2);
		  }
	  }
	  exit(0);
	    
}

			 

