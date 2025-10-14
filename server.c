#define _POSIX_C_SOURCE 200112L

#include <stdio.h>
#include <stdlib.h>

/* You will to add includes here */

#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <errno.h>
#include <arpa/inet.h>

#define BACKLOG 10
#define MAX_CLIENTS 100
#define BUF_SIZE 512

typedef struct 
{
  int sock;
  char nick[32];
  int active;
} Client;

Client clients[MAX_CLIENTS];

void remove_client(int idx) 
{
  if (idx >= 0 && idx < MAX_CLIENTS && clients[idx].active) 
  {
    printf("%s disconnected\n", clients[idx].nick);
    fflush(stdout);
    close(clients[idx].sock);
    clients[idx].active = 0;
    clients[idx].sock = -1;
  }   
}

int find_free_slot() 
{
  for (int i = 0; i < MAX_CLIENTS; i++) 
  {
    if (!clients[i].active) return i;
  }
  return -1;
}

int valid_nick(const char *nick) 
{
  size_t len = strlen(nick);
  if (len == 0 || len > 12) return 0;
  
  for (size_t i = 0; i < len; i++) 
  {
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

int main(int argc, char *argv[]) 
{

  /* Do magic :3 */

  if (argc != 2) 
  {
    fprintf(stderr, "Usage: %s host:port\n", argv[0]);
    exit(1);
  }

  char *sep = strchr(argv[1], ':');
  if (!sep) 
  {
    fprintf(stderr, "host:port required\n");
    exit(1);
  }

  char host[256];
  strncpy(host, argv[1], sep - argv[1]);
  host[sep - argv[1]] = '\0';
  char *port = sep + 1;

  struct addrinfo hints, *res;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  int status = getaddrinfo(host, port, &hints, &res);
  if (status != 0) 
  {
    fprintf(stderr, "ERROR: getaddrinfo failed: %s\n", gai_strerror(status));
    exit(1);
  }

  int sockfd = -1;
  for (struct addrinfo *p = res; p != NULL; p = p->ai_next) 
  {
    sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (sockfd < 0) continue;

    int yes = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

    if (bind(sockfd, p->ai_addr, p->ai_addrlen) == 0) break;

    close(sockfd);
    sockfd = -1;
  }

  if (sockfd < 0) 
  {
    fprintf(stderr, "Failed to bind\n");
    exit(1);
  }

  freeaddrinfo(res);

  if (listen(sockfd, BACKLOG) < 0) 
  {
    fprintf(stderr, "listen failed\n");
    exit(1);
  }

  for (int i = 0; i < MAX_CLIENTS; i++) 
  {
    clients[i].active = 0;
    clients[i].sock = -1;
  }

  printf("Server listening on %s:%s\n", host, port);
  fflush(stdout);

  fd_set master_set, read_set;
  FD_ZERO(&master_set);
  FD_SET(sockfd, &master_set);
  int max_fd = sockfd;

  while (1) 
  {
    max_fd = sockfd;
    for (int i = 0; i < MAX_CLIENTS; i++) 
    {
      if (clients[i].active && clients[i].sock > max_fd) 
      {
        max_fd = clients[i].sock;
      }
    }

    read_set = master_set;
    
    if (select(max_fd + 1, &read_set, NULL, NULL, NULL) < 0) 
    {
      fprintf(stderr, "ERROR: select failed: %s\n", strerror(errno));
      continue;
    }

    if (FD_ISSET(sockfd, &read_set)) 
    {
      struct sockaddr_storage cli_addr;
      socklen_t clilen = sizeof(cli_addr);
      int newsock = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
      
      if (newsock >= 0) 
      {
        int slot = find_free_slot();
        if (slot < 0) 
        {
          fprintf(stderr, "Max clients reached\n");
          close(newsock);
        } 
        else 
        {
          send(newsock, "HELLO 1\n", 8, 0);
          
          clients[slot].sock = newsock;
          clients[slot].active = 1;
          strcpy(clients[slot].nick, "pending");
          
          FD_SET(newsock, &master_set);
          if (newsock > max_fd) max_fd = newsock;
          
          printf("New connection on socket %d\n", newsock);
          fflush(stdout);
        }
      }
    }

    for (int i = 0; i < MAX_CLIENTS; i++) 
    {
      if (!clients[i].active) continue;
      
      int sock = clients[i].sock;
      if (FD_ISSET(sock, &read_set)) 
      {
        char buf[BUF_SIZE];
        int n = recv(sock, buf, sizeof(buf) - 1, 0);
        
        if (n <= 0) 
        {
          FD_CLR(sock, &master_set);
          remove_client(i);
          continue;
        } 
        else 
        {
          buf[n] = '\0';
          size_t len = strlen(buf);
          while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r')) 
          {
            buf[--len] = '\0';
          }
          
          if (strcmp(clients[i].nick, "pending") == 0) 
          {
            if (strncmp(buf, "NICK ", 5) == 0) 
            {
              char newnick[32];
              sscanf(buf + 5, "%31s", newnick);

              if (!valid_nick(newnick)) {
                send(sock, "ERR invalid nickname\n", 23, 0);
                FD_CLR(sock, &master_set);
                remove_client(i);
                continue;
              } 
              else 
              {
                strcpy(clients[i].nick, newnick);
                send(sock, "OK\n", 3, 0);
                printf("Client %s connected (socket %d)\n", clients[i].nick, sock);
                fflush(stdout);
              }
            } 
            else 
            {
              send(sock, "ERR invalid protocol\n", 23, 0);
              continue;
            }
          }
          else 
          {
            if (strncmp(buf, "MSG ", 4) == 0) 
            {
              char *msg = buf + 4;
              printf("%s: %s\n", clients[i].nick, msg);
              char buf[BUF_SIZE];
              snprintf(buf, sizeof(buf), "MSG %s %s\n", clients[i].nick, msg);
              for (int j = 0; j < MAX_CLIENTS; j++) 
              {
                if (clients[j].active) 
                {
                  send(clients[j].sock, buf, strlen(buf), 0);
                }
              }
              fflush(stdout);
            }
          }
        }
      }
    }
  }   

  close(sockfd);
  return 0;
}       