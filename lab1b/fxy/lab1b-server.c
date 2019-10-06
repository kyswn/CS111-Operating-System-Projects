//NAME: Xuanyu Fang
//EMAIL: fangmax98@g.ucla.edu
//ID: 504933797

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


pid_t childPid;
int pipeToShell[2];
int pipeToTerm[2];
int compressFlag = 0;
int socketfd1;
int socketfd2;
z_stream in_shell;
z_stream shell_to_stdout;

void shutServer()
{
  close(pipeToShell[1]);
  close(pipeToTerm[0]);
  close(socketfd1);
  close(socketfd2);
  int status;
  if (waitpid(childPid, &status, 0) == -1)
    {
      fprintf(stderr, "Error. Failed at waiting for child.\n");
      exit(1);
    }

  if (WIFEXITED(status))
    {
      fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n", WTERMSIG(status), WEXITSTATUS(status));
      exit(0);
    }
}

/*void signal_handler(int signum)
{
if (signum == SIGPIPE)
{
exit(1);
}
else if (signum == SIGINT)
{
kill(child_pid, SIGINT);
}
}*/


int main(int argc, char *argv[])
{
  atexit(shutServer);
  //signal(SIGPIPE, signal_handler);
  //signal(SIGINT, signal_handler);
  int port = -1;
  int port_num=-1;
  socklen_t  client_length;
  struct sockaddr_in server_address, client_address;

  static struct option options[] = {
    { "port",1, NULL,1 },
    { "compress",0, NULL,2 },
    { 0,0,0,0 }
  };

  int optret= getopt_long(argc, argv, "", options, NULL);
  while (optret != -1)
    {
      if (optret == 1)
	{
	  port = 1;
	  port_num = atoi(optarg);
	}
      else if (optret == 2)
	{
	  compressFlag = 1;
	}
      else
	{
	  fprintf(stderr, "Error. Invalid argument.\n");
	  exit(1);
	}
      optret = getopt_long(argc, argv, "", options, NULL);
    }
  if (port < 0)
    {
      fprintf(stderr, "Error! Mandatory port command. Correct usage: --port=port# \n");
      exit(1);
    }
  if (port_num < 0)
    {

      fprintf(stderr, "Error! The port information is mandatory. Correct usage: --port=port# \n");
      exit(1);
    }

  //setup socket
  socketfd1 = socket(AF_INET, SOCK_STREAM, 0);
  if (socketfd1 < 0) 
    {
      fprintf(stderr, "Error. Failed to open socket.\n");
      exit(1);
    }
  server_address.sin_family = AF_INET;
  server_address.sin_port = htons(port_num);
  server_address.sin_addr.s_addr = INADDR_ANY;
  if (bind(socketfd1, (struct sockaddr *) &server_address, sizeof(server_address)) < 0)
    {
      fprintf(stderr, "Error. Failed binding socket");
      exit(1);
    }

  /////////check
  listen(socketfd1, 5);
  client_length = sizeof(client_address);
  socketfd2 = accept(socketfd1, (struct sockaddr *) &client_address, &client_length);
  if (socketfd2 < 0) {
    fprintf(stderr, "error accepting client");
    exit(1);
  }



  if (pipe(pipeToShell) == -1 || pipe(pipeToTerm) == -1)
    {
      fprintf(stderr, "Error. Failed to create pipe.");
      exit(1);
    }
  childPid = fork();
  if (childPid < 0)
    {
      fprintf(stderr, "fork failed\n");
      exit(1);
    }
  else if (childPid > 0) //parent's work
    {
      close(pipeToShell[0]);
      close(pipeToTerm[1]);
      struct pollfd pollist[2];
      pollist[0].fd = socketfd2;
      pollist[1].fd = pipeToTerm[0];
      pollist[0].events = POLLIN | POLLHUP | POLLERR;
      pollist[1].events = POLLIN | POLLHUP | POLLERR;

      unsigned char buf[512];
      unsigned char compression_buf[512];
      int compression_buf_size = 512;
      int pollret;
      char c;
      //char cr = '\r';
      char lf = '\n';

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
	      int readret = read(pollist[0].fd, &buf, 512 * sizeof(char));
	      if (readret < 0) {
		fprintf(stderr, "Error. client read failed.\n");
		exit(1);
	      }
	      
	      if (compressFlag)  //decompress
		{
		  shell_to_stdout.zalloc = Z_NULL;
		  shell_to_stdout.zfree = Z_NULL;
		  shell_to_stdout.opaque = Z_NULL;

		  if (inflateInit(&shell_to_stdout) != Z_OK) {
		    fprintf(stderr, "error on inflateInit");
		    exit(1);
		  }
		  shell_to_stdout.avail_in = readret;
		  shell_to_stdout.next_in = buf;
		  shell_to_stdout.avail_out = compression_buf_size;
		  shell_to_stdout.next_out = compression_buf;

		  do 
		    {
		      if (inflate(&shell_to_stdout, Z_SYNC_FLUSH) != Z_OK) 
			{
			  fprintf(stderr, "error inflating");
			  exit(1);
			}
		    } 
		  while (shell_to_stdout.avail_in > 0);

		  inflateEnd(&shell_to_stdout);

		  int bytesAftercCompress = compression_buf_size - shell_to_stdout.avail_out;
		  int i = 0;
		  for (; i < bytesAftercCompress; i++)
		    {
		      c = compression_buf[i];
		      if (c == '\4')
			{
			  
			  close(pipeToShell[1]);
			  break;
			}
		      else if (c == '\3')
			{
			  
			  kill(childPid, SIGINT);
			}

		      else if (c == '\r' || c == '\n')
			{
			  
			  write(pipeToShell[1], &lf, 1);
			}

		      else
			{
			  
			  write(pipeToShell[1], &c, 1);
			}
		    }
		  memset(compression_buf, 0, bytesAftercCompress);
		}
	      else //no decompression needed
		{
		  int i = 0;
		  for (; i < readret; i++)
		    {
		      c = buf[i];
		      if (c == '\4')
			{
			  
			  close(pipeToShell[1]);
			  break;
			}
		      else if (c == '\3')
			{
			  kill(childPid, SIGINT);
			}
		      else if (c == '\r' || c == '\n')
			{
			  
			  write(pipeToShell[1], &lf, 1);
			}

		      else
			{
			  write(pipeToShell[1], &c, 1);
			}
		    }
		}
	      // potential memset
	    }
	  if ((pollist[1].revents & POLLIN) == POLLIN)//receive from shell
	    {
	      int readret = read(pollist[1].fd, &buf, 512 * sizeof(char));
	      if (readret < 0) {
		fprintf(stderr, "Error. Keyboard read failed.\n");
		exit(1);
	      }
	      if (compressFlag)
		{
		  

		  in_shell.zalloc = Z_NULL;
		  in_shell.zfree = Z_NULL;
		  in_shell.opaque = Z_NULL;

		  if (deflateInit(&in_shell, Z_DEFAULT_COMPRESSION) != Z_OK) {
		    fprintf(stderr, "error on deflateInit");
		    exit(1);
		  }

		  in_shell.avail_in = readret;
		  in_shell.next_in = buf; //return from shell
		  in_shell.avail_out = compression_buf_size;
		  in_shell.next_out = compression_buf; //compressed buff here
		  do {
		    if (deflate(&in_shell, Z_SYNC_FLUSH) != Z_OK) {
		      fprintf(stderr, "error deflating");
		      exit(1);
		    }
		  } while (in_shell.avail_in > 0);
		  write(socketfd2, &compression_buf, compression_buf_size - in_shell.avail_out);
		  deflateEnd(&in_shell);
		  int i = 0;
		  for (; i < readret; i++)
		    {
		      c = buf[i];
		      if (c == '\4')
			{

			  close(pipeToShell[1]);
			  exit(0);
			}

		    }

		}
	      else
		{
		  int i = 0;
		  for (; i < readret; i++)
		    {
		      c = buf[i];
		      if (c == '\4')
			{

			  close(pipeToShell[1]);
			  exit(0);
			}
		      else
			{
			  write(socketfd2, &c, 1);
			}
		    }
		}
	    }
	  if (((pollist[1].revents & POLLERR) == POLLERR) || ((pollist[1].revents & POLLHUP) == POLLHUP))
	    {
	      exit(0);
	    }
	  if (((pollist[0].revents & POLLERR) == POLLERR) || ((pollist[0].revents & POLLHUP) == POLLHUP))
	    {
	      close(pipeToShell[1]);
	    }
	}



    }
  else //childpid==0, children's work
    {
      close(pipeToShell[1]);
      close(pipeToTerm[0]);

      dup2(pipeToShell[0], 0);
      dup2(pipeToTerm[1], 1);

      close(pipeToShell[0]);
      close(pipeToTerm[1]);

      char a[] = "/bin/bash";
      char *args[2] = { a, NULL };
      if (execvp(a, args) == -1)//run shell
	{
	  fprintf(stderr, "Error: %s", strerror(errno));
	  exit(1);
	}
    }
  exit(0);
}
