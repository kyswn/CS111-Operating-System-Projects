/* NAME: Shawn Ma */
/* EMAIL: breadracer@outlook.com */
/* ID: 204996814 */

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <math.h>
#include <mraa/gpio.h>
#include <mraa/aio.h>
#include <getopt.h>
#include <poll.h>
#include <time.h>
#include <string.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>

sig_atomic_t volatile run_flag = 1;

char const CENTEGRADE = 'C';
char const FAHRENHEIT = 'F';

int const B = 4275;               // B value of the thermistor
int const R0 = 100000;            // R0 = 100k

enum cmd_line_options {SCALE, PERIOD, LOG, ID, HOST};

struct states {
  char volatile scale_flag;
  int volatile period;
  int volatile stop_flag;
  int log_flag;
  int buf_tail;
  int buf_head;
  int lenbuf;
  char *id;
  char *host;
  int port;
  int socket_fd;
  FILE *log_fd;
  SSL_CTX *ctx;
  SSL *ssl;
  SSL_METHOD const *method;
};

// helper functions
void error(char *message, int code) {
  fprintf(stderr, "%s\n", message);
  exit(code);
}

void sig_handler(int sig) {
  if (sig == SIGINT)
    run_flag = 0;
}

float ctof(float c) {
  return c * 9.0 / 5.0 + 32.0;
}

float get_temperature(int analog_read) {
  float R = 1023.0 / analog_read - 1.0;
  R = R0 * R;
  float temperature = 1.0 / (log(R / R0) / B + 1.0 / 298.15) - 273.15; 
  return temperature;
}

void report_temperature(uint16_t value, struct states *state) {
  time_t now = time(0);
  char time_str[9];
  strftime(time_str, sizeof(time_str), "%T", localtime(&now));
  float temperature = get_temperature(value);
  if (state->scale_flag == FAHRENHEIT)
    temperature = ctof(temperature);
  int length_buf = (int)log10(temperature) + 16;
  char *report_buf = malloc(sizeof(char) * length_buf);
  if (!report_buf)
    error("malloc error", 1);
  sprintf(report_buf, "%s %.1f\n", time_str, temperature);
  SSL_write(state->ssl, report_buf, strlen(report_buf));
  free(report_buf);
  fprintf(state->log_fd, "%s %.1f\n", time_str, temperature);
  fflush(state->log_fd);
}

int main(int argc, char **argv) {
  // declare and initialize variables
  struct states state = {FAHRENHEIT, 1, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0};
  char *buffer = malloc(sizeof(char) * 32);
  if (!buffer)
    error("malloc error", 1);
  
  // configure command-line options
  int opt, optindex;
  struct option options[] = {
    {"scale", required_argument, 0, SCALE},
    {"period", required_argument, 0, PERIOD},
    {"log", required_argument, 0, LOG},
    {"id", required_argument, 0, ID},
    {"host", required_argument, 0, HOST},
    {0, 0, 0, 0}    
  };  

  // command line options processing
  while (1) {
    opt = getopt_long(argc, argv, "", options, &optindex);
    if (opt == -1)
      break;
    switch (opt) {
    case SCALE:
      if (strlen(optarg) > 1 || (optarg[0] != CENTEGRADE && optarg[0] != FAHRENHEIT))
	error("Invalid scale", 1);
      state.scale_flag = optarg[0];
      break;
    case PERIOD:
      if (atoi(optarg) <= 0)
	error("Invalid period number", 1);
      state.period = atoi(optarg);
      break;
    case LOG: {
      state.log_fd = fopen(optarg, "w");
      state.log_flag = 1;
      break;
    }
    case ID:
      state.id = optarg;
      break;
    case HOST:
      state.host = optarg;
      break;
    case '?':
      error("Invalid command-line parameter", 1);
    }
  }
  state.port = atoi(argv[argc - 1]);
  
  if (!state.log_flag || !state.id || !state.host)
    error("Missing one or more mandatory options", 1);

  OpenSSL_add_all_algorithms();
  SSL_load_error_strings();
  SSL_library_init();
  state.method = SSLv23_client_method();
  state.ctx = SSL_CTX_new(state.method);
  if (!state.ctx)
    error("SSL_CTX_new error", 1);
  state.ssl = SSL_new(state.ctx);
  
  state.socket_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (state.socket_fd < 0)
    error("socket error", 1);
  
  struct hostent *host_info = gethostbyname(state.host);
  if (!host_info)
    error("gethostbyname error", 1);

  struct sockaddr_in server;
  memset(&server, 0, sizeof(server));
  memcpy(&server.sin_addr.s_addr, host_info->h_addr_list[0], host_info->h_length);
  server.sin_family = AF_INET;
  server.sin_port = htons(state.port);
  if (connect(state.socket_fd , (struct sockaddr*)&server , sizeof(server)) < 0)
    error("connect error", 2);

  SSL_set_fd(state.ssl, state.socket_fd);
  if(SSL_connect(state.ssl) != 1) {
    error("SSL_connect error", 2);
  }

  char id_buf[64];
  sprintf(id_buf, "ID=%s\n", state.id);
  SSL_write(state.ssl, id_buf, strlen(id_buf));
  fprintf(state.log_fd, "ID=%s\n", state.id);
  fflush(state.log_fd);
  
  // get input
  mraa_aio_context sensor;
  sensor = mraa_aio_init(1);

  signal(SIGINT, sig_handler);
  
  struct pollfd stdin_fd = {state.socket_fd, POLLIN, 0};


  time_t now = time(0);
  while (run_flag) {
    poll(&stdin_fd, 1, 0);
    if (stdin_fd.revents & POLLIN) {
      char buf;
      while (SSL_read(state.ssl, &buf, 1)) {
	buffer[state.buf_tail++] = buf;
	if (state.buf_tail == state.lenbuf - 1) {
	  char *new_buffer = realloc(buffer, sizeof(char) * state.lenbuf * 2);
	  if (!new_buffer)
	    error("realloc error", 1);
	  buffer = new_buffer;
	  state.lenbuf *= 2;
	}
	buffer[state.buf_tail] = 0;
	// check arguments
	if (buf == '\n') {
	  fprintf(state.log_fd, "%s", buffer + state.buf_head);
	  fflush(state.log_fd);
	  if (strcmp(buffer + state.buf_head, "OFF\n") == 0) {
	    run_flag = 0;
	    char t_str_shutdown[9];
	    time_t t_shutdown = time(0);
	    strftime(t_str_shutdown, sizeof(t_str_shutdown), "%T", localtime(&t_shutdown));
	    char temp_buf[20];
	    sprintf(temp_buf, "%s SHUTDOWN\n", t_str_shutdown);
	    SSL_write(state.ssl, temp_buf, strlen(temp_buf));
	    fprintf(state.log_fd, "%s SHUTDOWN\n", t_str_shutdown);
	    fflush(state.log_fd);
	  }
	  else if (strcmp(buffer + state.buf_head, "SCALE=C\n") == 0)
	    state.scale_flag = CENTEGRADE;
	  else if (strcmp(buffer + state.buf_head, "SCALE=F\n") == 0)
	    state.scale_flag = FAHRENHEIT;
	  else if (strcmp(buffer + state.buf_head, "STOP\n") == 0)
	    state.stop_flag = 1;
	  else if (strcmp(buffer + state.buf_head, "START\n") == 0)
	    state.stop_flag = 0;	  
	  else if (strstr(buffer + state.buf_head, "PERIOD=") == buffer + state.buf_head) {
	    int length = state.buf_tail - state.buf_head;
	    char *period_str = malloc(sizeof(char) * (length - 7));
	    if (!period_str)
	      error("malloc error", 1);
	    strncpy(period_str, buffer + state.buf_head + 7, length - 8);
	    period_str[length - 8] = 0;
	    int period = atoi(period_str);
	    if (period > 0)
	      state.period = period;
	  }
	  else if (!strstr(buffer + state.buf_head, "LOG "))
	    fprintf(stderr, "Invalid command: %s", buffer + state.buf_head);
	  state.buf_head = state.buf_tail;
	  break;
	}
	if (time(0) - now >= state.period && !state.stop_flag) {
	  report_temperature(mraa_aio_read(sensor), &state);
	  now = time(0);
	}
      }
    }
    if (time(0) - now >= state.period && !state.stop_flag) {
      report_temperature(mraa_aio_read(sensor), &state);
      now = time(0);
    }
    if (run_flag == 0) {
      mraa_aio_close(sensor);
      exit(0);
    }
  }


  mraa_aio_close(sensor);
  return 0;
}

