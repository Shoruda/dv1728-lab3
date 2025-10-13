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

int valid_nick(const char *nick) {
  size_t len = strlen(nick);
  if (len == 0 || len > 12) return 0;

  for (size_t i = 0; i < len; i++) {
    char c = nick[i];
    if (!((c >= 'A' && c <= 'Z') ||
        (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') ||
        c == '_' || c == ')')) {
      return 0;
    }
  }
  return 1;
}

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

  if (!valid_nick(nickname)) {
    fprintf(stderr, "ERROR: invalid nickname. Only A-Z, a-z, 0-9, _, ) allowed, max 12 chars\n");
    exit(1);
}

  struct addrinfo hints, *res;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  int status = getaddrinfo(server_ip, port, &hints, &res);
  if (status != 0) 
  {
    fprintf(stderr, "ERROR: getaddrinfo failed: %s\n", gai_strerror(status));
    exit(1);
  }

  int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (sockfd < 0) 
  {
    fprintf(stderr, "ERROR: socket creation failed: %s\n", strerror(errno));
    freeaddrinfo(res);
    exit(1);
  }

  if (connect(sockfd, res->ai_addr, res->ai_addrlen) < 0) 
  {
    fprintf(stderr, "ERROR: connect failed: %s\n", strerror(errno));
    freeaddrinfo(res);
    close(sockfd);
    exit(1);
  }

  freeaddrinfo(res);
  printf("Connected to %s:%s as %s\n", server_ip, port, nickname);

  char buffer[BUF_SIZE];
  int bytes = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
  if (bytes <= 0) {
    fprintf(stderr, "ERROR: server closed connection before handshake\n");
    close(sockfd);
    exit(1);
  }
  buffer[bytes] = '\0';

  if (strncmp(buffer, "HELLO 1\n", 9) != 0) 
  {
    fprintf(stderr, "ERROR: unexpected handshake from server: %s\n", buffer);
    close(sockfd);
    exit(1);
  }

  printf("Handshake OK: %s", buffer);
  fflush(stdout);
  snprintf(buffer, sizeof(buffer), "NICK %s\n", nickname);
  if (send(sockfd, buffer, strlen(buffer), 0) < 0) 
  {
    fprintf(stderr, "ERROR: send failed: %s\n", strerror(errno));
    close(sockfd);
    exit(1);
  }

  bytes = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
  if (bytes <= 0) {
    fprintf(stderr, "ERROR: server closed connection after NICK\n");
    close(sockfd);
    exit(1);
  }
  buffer[bytes] = '\0';

  if (strncmp(buffer, "OK\n", 3) != 0) {
    fprintf(stderr, "ERROR: expected OK from server, got: %s\n", buffer);
    close(sockfd);
    exit(1);
  }
  printf("%s", buffer);
  fflush(stdout);

  tcgetattr(STDIN_FILENO, &t);
  t.c_lflag &= ~(ECHO | ICANON);
  tcsetattr(STDIN_FILENO, TCSANOW, &t);

  printf("You can now type messages. Press Ctrl+C to exit.\n");
  fflush(stdout);

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
      fprintf(stderr, "ERROR: select failed: %s\n", strerror(errno));
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
      buffer[bytes] = '\0';
      buffer[strcspn(buffer, "\n")] = '\0';

      if (strncmp(buffer, "MSG ", 4) == 0) {
        printf("%s\n", buffer + 4);
      } else {
        printf("%s\n", buffer);
      }
      fflush(stdout);
    }

    if (FD_ISSET(STDIN_FILENO, &readfds)) {
      memset(buffer, 0, sizeof(buffer));
      if (fgets(buffer, sizeof(buffer), stdin) != NULL) 
      {
        buffer[strcspn(buffer, "\n")] = 0;
        if (strlen(buffer) > 255) {
          fprintf(stderr, "ERROR: message too long (max 255 characters)\n");
        } else 
        {
          char msg[BUF_SIZE];
          snprintf(msg, sizeof(msg), "MSG %s\n", buffer);
          send(sockfd, msg, strlen(msg), 0);
          fflush(stdout);
        }
      }
    }
  }

  tcgetattr(STDIN_FILENO, &t);
  t.c_lflag |= (ECHO | ICANON);
  tcsetattr(STDIN_FILENO, TCSANOW, &t);

  close(sockfd);
  return 0;

}
