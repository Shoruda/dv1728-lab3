#define _POSIX_C_SOURCE 200112L

#include <stdio.h>
#include <stdlib.h>

/* You will to add includes here */

#include <sys/select.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <termios.h>
#include <fcntl.h>

#define BUF_SIZE 1024

struct termios t;

int main(int argc, char *argv[]){
  
  /* Do magic */

  if (argc != 3)
  {
    fprintf(stderr, "Usage: %s <server_ip:port> <nickname>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  char *address = argv[1];
  char *sep = strchr(address, ':');
  if (!sep) 
  {
    fprintf(stderr, "Error: address must be in format IP:PORT\n");
    exit(EXIT_FAILURE);
  }

  *sep = '\0';
  char *server_ip = address;
  char *port = sep + 1;
  char *nickname = argv[2];

  struct addrinfo hints, *res;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  int status = getaddrinfo(server_ip, port, &hints, &res);
  if (status != 0) 
  {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
    exit(EXIT_FAILURE);
  }

  int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (sockfd < 0) 
  {
    perror("socket");
    freeaddrinfo(res);
    exit(EXIT_FAILURE);
  }

  if (connect(sockfd, res->ai_addr, res->ai_addrlen) < 0) 
  {
    perror("connect");
    freeaddrinfo(res);
    close(sockfd);
    exit(EXIT_FAILURE);
  }

  freeaddrinfo(res);
  printf("Connected to %s:%s as %s\n", server_ip, port, nickname);

  char buffer[BUF_SIZE];
  snprintf(buffer, sizeof(buffer), "NICK %s\n", nickname);
  send(sockfd, buffer, strlen(buffer), 0);

  tcgetattr(STDIN_FILENO, &t);
  t.c_lflag &= ~(ECHO | ICANON);
  tcsetattr(STDIN_FILENO, TCSANOW, &t);

  printf("You can now type messages. Press Ctrl+C to exit.\n");

  fd_set readfds;
  while (1) 
  {
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);
    FD_SET(sockfd, &readfds);
    int maxfd = (STDIN_FILENO > sockfd) ? STDIN_FILENO : sockfd;

    int activity = select(maxfd + 1, &readfds, NULL, NULL, NULL);
    if (activity < 0) 
    {
      perror("select");
      break;
    }

    if (FD_ISSET(sockfd, &readfds)) 
    {
      memset(buffer, 0, sizeof(buffer));
      int bytes = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
      if (bytes <= 0) {
        printf("\nServer disconnected.\n");
        break;
      }
      printf("\r%s\n> ", buffer);
      fflush(stdout);
    }

    if (FD_ISSET(STDIN_FILENO, &readfds)) {
      memset(buffer, 0, sizeof(buffer));
      if (fgets(buffer, sizeof(buffer), stdin) != NULL) 
      {
        buffer[strcspn(buffer, "\n")] = 0;
        char msg[BUF_SIZE];
        snprintf(msg, sizeof(msg), "MSG %s\n", buffer);
        send(sockfd, msg, strlen(msg), 0);
        printf("> ");
        fflush(stdout);
      }
    }
  }

  tcgetattr(STDIN_FILENO, &t);
  t.c_lflag |= (ECHO | ICANON);
  tcsetattr(STDIN_FILENO, TCSANOW, &t);

  close(sockfd);
  return 0;

}
