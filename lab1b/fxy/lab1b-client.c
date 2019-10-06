//NAME: Xuanyu Fang
//EMAIL: fangmax98@g.ucla.edu
//ID: 504933797	       


#define _GNU_SOURCE
#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>
#include <string.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <zlib.h>

struct termios attrSaved;
int hasport=0;
int needlog=0;
int needcompression=0;
char* logFile = NULL;
z_stream streamIn;
z_stream streamOut;
int logc;
int socketfd;
int log_fd;
void restore()
{
  tcsetattr(STDIN_FILENO, TCSANOW, &attrSaved);
  
  
  
  /*if (needlog)
    {
    close(log_fd);
    }
    close(socketfd);*/
  
}
void setmode()
{
  struct termios a;
  tcgetattr(0, &attrSaved);
  atexit(restore);

  tcgetattr(0, &a);
  a.c_iflag = ISTRIP;/* only lower 7 bits*/
  a.c_oflag = 0;/* no processing*/
  a.c_lflag = 0;/* no processing*/
  int result = tcsetattr(0, TCSANOW, &a);
  if (result < 0)
    {
      fprintf(stderr, "Error. Failed to set terminal mode to non-canonical.");
      exit(1);
    }
}



int main(int argc, char *argv[])
{
  
  
  int portnum;
  struct option options[] =
    {
      { "port", required_argument, NULL, 1 },
      { "log", required_argument, NULL, 2 },
      { "compress", 0, NULL, 3 },
      { 0,0,0,0 }
    };

  int ret = getopt_long(argc, argv, "", options, NULL);

  while (ret != -1)
    {
      switch (ret)
	{
	case 1:
	  hasport = 1;
	  portnum = atoi(optarg);
	  break;
	case 2:
	  needlog = 1;
	  logFile = optarg;
	  log_fd = open(logFile, O_WRONLY | O_CREAT | O_TRUNC, 0666);
	  if (log_fd < 0)
	    {
	      fprintf(stderr, "ERROR IN CREATING LOG FILE: %s", strerror(errno));
	      exit(1);
	    }
	  break;
	case 3:
	  needcompression = 1;
	  break;
	default:
	  fprintf(stderr, "error: invalid argument\n");
	  exit(1);
	}
      ret = getopt_long(argc, argv, "", options, NULL);
    }
  
  struct sockaddr_in server_address;
  struct hostent *server;
  socketfd = socket(AF_INET, SOCK_STREAM, 0);
  if (socketfd < 0) {
    fprintf(stderr, "ERROR IN CREATING SOCKET");
    exit(1);
  }
  
  server = gethostbyname("localhost");    //local host in IPV4
  if (server == NULL) {
    fprintf(stderr, "error finding host\n");
    exit(1);
  }

  memset((char*)&server_address, 0, sizeof(server_address));
  server_address.sin_family = AF_INET;
  memcpy((char*)&server_address.sin_addr.s_addr, (char*)server->h_addr, server->h_length);
  server_address.sin_port = htons(portnum);

  if (connect(socketfd, (struct sockaddr*) &server_address, sizeof(server_address)) < 0)
    {
      fprintf(stderr, "Error! Connect failed.\n");
      exit(1);
    }
  setmode();
  if (hasport < 0)
    {
      fprintf(stderr, "Error! The port command is mandatory. Correct usage: --port=port#\n");
      exit(1);
    }

  struct pollfd pollist[2];
  pollist[0].fd = 0;
  pollist[1].fd = socketfd;
  pollist[0].events = POLLIN | POLLHUP | POLLERR;
  pollist[1].events = POLLIN | POLLHUP | POLLERR;

  unsigned char buf[512];
  unsigned char compression_buf[512];
  int compression_buf_size = 512;
  int pollret;
  char c;
  char cr = '\r';
  char lf = '\n';
  char crlf[2] = { '\r','\n' };

  while (1)
    {
      pollret = poll(pollist, 2, 0);
      if (pollret < 0)
	{
	  fprintf(stderr, "Error: Polling failed.\n");
	  exit(1);
	}

      if ((pollist[0].revents & POLLIN) == POLLIN)
	{
	  int readret = read(0, buf, sizeof(char) * 512);
	  if (readret < 0) {
	    fprintf(stderr, "Error: failed to read from keyboard.\n");
	    exit(1);
	  }
	  if (needcompression && readret > 0)
	    {
	      streamIn.zalloc = Z_NULL;
	      streamIn.zfree = Z_NULL;
	      streamIn.opaque = Z_NULL;

	      if (deflateInit(&streamIn, Z_DEFAULT_COMPRESSION) != Z_OK) {
		fprintf(stderr, "Error. Failed to Initialize Compression");

		exit(1);
	      }

	      streamIn.avail_out = compression_buf_size;
	      streamIn.next_out = compression_buf;
	      streamIn.avail_in = readret;
	      streamIn.next_in = buf;

	      do {

		if (deflate(&streamIn, Z_SYNC_FLUSH) != Z_OK) {
		  fprintf(stderr, "Error. Failed deflating.\n");
		  exit(1);
		}
	      } while (streamIn.avail_in > 0);
	      //write to socket
	      write(socketfd, &compression_buf, compression_buf_size - streamIn.avail_out);
	      //write to log
	      if (needlog)
		{
		  int num = compression_buf_size - streamIn.avail_out;
		  //logcount = sprintf(logbuffer, "RECEIVED %d bytes: %s\n", (int)(count * sizeof(char)), buffer);
		  //int i = 0;
		  /*for (; i < num; i++)
		    {
		    char cur = compression_buf[i];
		    write(log_fd, "SENT 1 byte: ", 13);
		    write(log_fd, &cur, sizeof(char));
		    write(log_fd, &lf, sizeof(char));
		    }*/
		  char beginpart[14] = "SENT x bytes: ";
		  beginpart[5] = '0' + num;
		  write(log_fd, beginpart, 14);
		  write(log_fd, compression_buf, num);
		  write(log_fd, "\n", 1);
		  
		}
	      deflateEnd(&streamIn);
	      /////////////write to output
	      int i = 0;
	      for (; i < readret; i++)
		{
		  c = buf[i];
		  if (c == '\r' || c == '\n')
		    {
		      write(1, &cr, 1);
		      write(1, &lf, 1);
		    }
		  else
		    {
		      write(1, &c, 1);
		    }
		}
	    }
	  else
	    {
	      if (needlog)//make log
		{
		  //int i = 0;
		  /*for (; i < readret; i++)
		    {
		    char cur = buf[i];
		    write(log_fd, "SENT 1 byte: ", 13);
		    write(log_fd, &cur, sizeof(char));
		    write(log_fd, &lf, sizeof(char));
		    }*/
		  char beginpart[14] = "SENT x bytes: ";
		  beginpart[5] = '0' + readret;
		  write(log_fd, beginpart, 14);
		  write(log_fd, buf, readret);
		  write(log_fd, "\n", 1);
		}
	      int i = 0;
	      for (; i < readret; i++) 
		{
		  char cur = buf[i];
		  if (cur == 0X0A || cur == 0X0D)
		    {
		      write(1, &crlf, 2);
		      
		      write(socketfd, &cur, sizeof(char));
		    }
		  else
		    {
		      write(1, &cur, sizeof(char));
		      
		      write(socketfd, &cur, sizeof(char));
		      
		    }
		}
	    }
	}
      
      if ((pollist[1].revents & POLLIN) == POLLIN)
	{
	  int readret = read(socketfd, buf, 512 * sizeof(char));
	  if (readret < 0) {
	    fprintf(stderr, "ERROR IN READ FROM KEYBOARD. \n");
	    exit(1);
	  }
	  if (readret == 0)
	    {
	      exit(0);
	    }
	  if (needlog) 
	    {
	      //int i = 0;
	      /*for (; i < readret; i++)
		{
		char cur = buf[i];
		write(log_fd, "RECEIVED 1 byte: ", 17);
		write(log_fd, &cur, sizeof(char));
		write(log_fd, &lf, sizeof(char));
		}*/
	      char beginpart[18] = "RECEIVED x bytes: ";
	      beginpart[9] = '0' + readret;
	      write(log_fd, beginpart, 18);
	      write(log_fd, buf, readret);
	      write(log_fd, "\n", 1);
	    }

	  if (needcompression&& readret > 0) {
	    streamOut.zalloc = Z_NULL;
	    streamOut.zfree = Z_NULL;
	    streamOut.opaque = Z_NULL;

	    if (inflateInit(&streamOut) != Z_OK) {
	      fprintf(stderr, "ERROR IN Initialize DECOMPRESSION");

	      exit(1);
	    }


	    streamOut.avail_out = compression_buf_size;
	    streamOut.next_out = compression_buf;
	    streamOut.avail_in = readret;
	    streamOut.next_in = buf;


	    do {
	      if (inflate(&streamOut, Z_SYNC_FLUSH) != Z_OK) {
		fprintf(stderr, "Error! Failed Decompression.\n");
		
		exit(1);
	      }
	    } while (streamOut.avail_in > 0);

	    inflateEnd(&streamOut);

	    int size = compression_buf_size - streamOut.avail_out;
	    int i = 0;
	    for (; i < size; i++) {
	      char cur = compression_buf[i];
	      if (cur == 0X0A || cur == 0X0D) {
		write(1, &crlf, 2);
	      }
	      else {
		write(1, &cur, 1);
	      }

	    }
	  }
	  else
	    {
	      int i = 0;
	      for (; i < readret; i++) {
		char cur = buf[i];
		if (cur == 0X0A || cur == 0X0D) {
		  write(1, &crlf, 2);
		}
		else {
		  write(1, &cur, 1);
		}

	      }
	    }
	  memset(compression_buf, 0, readret);
	}
      if (((pollist[1].revents & POLLERR) == POLLERR) || ((pollist[1].revents & POLLHUP) == POLLHUP))
	{
	  //fprintf(stderr, "ERROR IN SHELL");
	  
	  exit(0);
	}
    }

  exit(0);
}
