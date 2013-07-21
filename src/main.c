#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>

#define APPNAME "internetisdownredirect"
#define APPVERSION "0.1"
#define LISTEN_PORT 3000
#define CONNECTION_MAX_BACKLOG 64
#define MAXRECV 4096
#define REDIRECTMSG "HTTP/1.1 302 See Other\nLocation: http://sudoroom.org/\n\n"
#define HTTP_REDIRECT_MSG "HTTP/1.1 302 See Other\nLocation: "

char* response;

void version() {
  fprintf(stderr, "%s version: %s", APPNAME, APPVERSION);
}

void usage(char* cmd_name) {
  fprintf(stderr, "%s: <port> <redirect_url> [debug]\n", cmd_name);
  fprintf(stderr, "    port is the port where the %s listens (required).", cmd_name);
  fprintf(stderr, "    redirect_url is the url to which http GET and POST are redirected (required).");
  fprintf(stderr, "    If debug is specified as the third argument, debug output will be enabled (optional).");
}

void fail(char* error) {
    perror(error);
    exit(-1);
}

void debug(char* dbg) {

}

int read_and_respond(int fd) {
  char buffer[MAXRECV];
  int nbytes;

  memset(buffer, 0, MAXRECV);
  
  nbytes = read(fd, buffer, MAXRECV);
  debug("Reading\n");

  if(nbytes < 0) { // ERROR
    close(fd);
    perror("read error");
    return -1;

  } else if(nbytes == 0) { // EOF
    debug("Connection closed\n");
    return -1;

  } 

  if((strncmp("GET", buffer, 3) == 0) || 
     (strncmp("POST", buffer, 4) == 0)) {
    
    if(send(fd, REDIRECTMSG, sizeof(REDIRECTMSG), 0) < 0) {
      perror("send error");
      return -1;
    }

    //    close(fd);
  } else {
    debug("unknown query\n");
    return -1;
  }


  return 0;
}

int run(port) {

  int listener;
  int keep_running = 1;
  int result;
  fd_set fds;
  fd_set cur_fds;

  listener = socket(AF_INET, SOCK_STREAM, 0);
  if(listener < 0) {
    fail("unable to open socket");
  }
  
  int one = 1;
  if(setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, (char*) &one, sizeof(one)) < 0) {
    close(listener);
    fail("failed to set SO_RESUEADDR on socket");
  }

  // Set non-blockingg
  int flags = fcntl(listener, F_GETFL, 0);
  if(flags == -1) {
    close(listener);
    fail("Failed to get socket flags");
  }

  if(fcntl(listener, F_SETFL, flags |= O_NONBLOCK) < 0) {
    close(listener);
    fail("Failed to set non-blocking mode");
  }

  struct sockaddr_in listen_addr;
  memset(&listen_addr, 0, sizeof(listen_addr));
  listen_addr.sin_family      = AF_INET;
  listen_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  listen_addr.sin_port        = htons(LISTEN_PORT);

  if(bind(listener, (struct sockaddr*) &listen_addr, sizeof(listen_addr)) < 0) {
    close(listener);
    fail("bind failed");
   }

  if(listen(listener, CONNECTION_MAX_BACKLOG) < 0) {
    close(listener);
    fail("listen failed");
  }

  FD_ZERO(&fds);
  FD_SET(listener, &fds);

  int max_fd = listener;

  struct timeval timeout;
  timeout.tv_sec = 10;
  timeout.tv_usec = 0;
  

  struct sockaddr_in client_addr;
  size_t size;

  while(keep_running) {

    cur_fds = fds;
    result = select(FD_SETSIZE, &cur_fds, NULL, NULL, &timeout);

    // reset timer
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
    
    if(result < 0) {
      close(listener);
      fail("select failed");
    } else if(result == 0) {
      // request timed out
    }

    int i;
    for(i=0; (i <= max_fd) && (result > 0); i++) {
      if(!FD_ISSET(i, &cur_fds)) {
        continue;
      }
      if(i == listener) { // connection request

        int con;
        size = sizeof(client_addr);
        con = accept(listener, (struct sockaddr *) &client_addr, &size);

        if(con < 0) {
          perror("accepting incoming connection failed");
          continue;
        }
        debug("Incoming connection accepted\n");

        //        fprintf(stderr,
        //                "Server: connect from host %s, port %hd.\n",
        //                inet_ntoa(client_addr.sin_addr),
        //                ntohs(client_addr.sin_port));

        FD_SET(con, &fds);
        if(con > max_fd) {
          max_fd = con;
        }

      } else { // data on connected socket
        if(read_and_respond(i) < 0) {
          close(i);
          FD_CLR(i, &fds);
        }
      }
    }
    

  }

  close(listener);

	return 0;
}



int main(int argc, char* argv[]) {
  
  if(argc >= 2) {
    if(strncmp("-h", argv[1], 2) == 0) {
      usage(argv[0]);
      return 0;
    } else if(strncmp("-v", argv[1], 2) == 0) {
      version(argv[0]);
      return 0;
    }
  }

  if(argc != 3) {
    usage(argv[0]);
    return -1;
  }

  int port = atoi(argv[1]);
  if((port < 1) || (port > 65535)) {
    fprintf(stderr, "Port out of range\n");
    usage(argv[0]);
    return -1;
  }

  response = malloc(strlen(HTTP_REDIRECT_MSG)+strlen(argv[2])+2);
  strncpy(response, HTTP_REDIRECT_MSG, strlen(HTTP_REDIRECT_MSG));
  strncpy(response+strlen(HTTP_REDIRECT_MSG), argv[2], strlen(argv[2]));
  snprintf(response+strlen(HTTP_REDIRECT_MSG)+strlen(argv[2]), 3, "\n\n\0");

  int ret = run(port);

  return ret;
  
}
