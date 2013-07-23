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
#include <httpd.h>

#define APPNAME "internetisdownredirect"
#define APPVERSION "0.1"

#define PROXY_HOST "localhost\0"

int debug_mode;
httpd* server;
char* redirect_url;
char* location_header;

void version() {
  fprintf(stderr, "%s version: %s", APPNAME, APPVERSION);
}

void usage(char* cmd_name) {
  fprintf(stderr, "%s: <ip> <port> <redirect_url> [debug]\n", cmd_name);
  fprintf(stderr, "    <ip> is the IP where %s binds and listens. Put ALL to bind to all IPs. \n", cmd_name);
  fprintf(stderr, "    <port> is the port where the %s listens (required).\n", cmd_name);
  fprintf(stderr, "    <redirect_url> is the url to which http GET and POST\n");
  fprintf(stderr, "     requests are redirected (required).\n");
  fprintf(stderr, "    If debug is specified as the third argument,\n");
  fprintf(stderr, "     debug output will be enabled (optional).");
}

void fail(char* error) {
    perror(error);
    exit(-1);
}

void debug(char* dbg) {
  if(debug_mode > 0) {
    printf("[debug]: %s", dbg);
  }
}

// do we have a host-specific redirect?
char* host_specific_redirect(char* hostname) {
  // TODO actually implement this
  return NULL;
}

void redirect() {

  char* hostname = NULL;
  char* end;
  int hostname_length = 0;
  char* location = NULL;

  // only respond to http get
  if(server->request.method != HTTP_GET) {
		httpdEndRequest(server);
  }

  // code for host-specific redirect
  // the idea is to redirect to mesh-hosted
  // alternatives to popular services
  // when the mesh is down
  // TODO not yet fully implemented
  if(server->request.host) {
    end = index(server->request.host, ':');
    if(!end) {
      end = index(server->request.host, '\r');
    }
    if(!end) {
      end = index(server->request.host, '\n');
    }
    if(!end) {
      end = index(server->request.host, '\0');
    }
    if(!end) {
      fail("could not parse hostname");
    }

    hostname_length = end - (server->request.host);
    hostname = malloc(hostname_length + 1);
    strncpy(hostname, server->request.host, hostname_length);
    hostname[hostname_length] = '\0';
    
    location = host_specific_redirect(hostname);
  }

  if(debug_mode) {
    if(hostname) {
      printf("Host: %s\n", hostname);
    } else {
      printf("Request did not contain a hostname.\n");
    }
    if(location) {
      printf("Host-specific redirect: %s\n", location);
    }
  }

  if(location) {
    printf("UNIMPLEMENTED: Host-specific redirect\n");
  }

  httpdSetResponse(server, "302 Found");
  httpdAddHeader(server, location_header);
  httpdAddHeader(server, "Connection: close");
  httpdSendHeaders(server);
}

int run(int port, char* ip, char* redirect_url) {

  int result;
	struct timeval timeout;


  if(strcmp(ip, "ALL") == 0) {
    server = httpdCreate(NULL, port);
  } else {
    server = httpdCreate(ip, port);
  }

	if(server == NULL) {
		fail("Can't create server");
	}

  if(debug_mode) {
    httpdSetAccessLog(server, stdout);
  }

	httpdSetErrorLog(server, stderr);

  httpdAddCWildcardContent(server, "/", NULL, redirect);

  while(1) {
		timeout.tv_sec = 5;
		timeout.tv_usec = 0;
		result = httpdGetConnection(server, &timeout);

		if(result == 0) {
			continue;
		}

		if(result < 0) {
      fprintf(stderr, "error opening incoming connection\n");
			continue;
		}

		if(httpdReadRequest(server) < 0) {
			httpdEndRequest(server);
			continue;
		}

		httpdProcessRequest(server);
		httpdEndRequest(server);
  }
}

int main(int argc, char* argv[]) {

  char* ip;

  debug_mode = 0;
  
  if(argc == 2) {
    if(strncmp("-h", argv[1], 2) == 0) {
      usage(argv[0]);
      return 0;
    } else if(strncmp("-v", argv[1], 2) == 0) {
      version(argv[0]);
      return 0;
    } else {
      usage(argv[0]);
      return -1;
    }
  }

  if((argc <= 1) || (argc > 5)) {
    usage(argv[0]);
    return -1;
  }

  int port = atoi(argv[2]);
  if((port < 1) || (port > 65535)) {
    fprintf(stderr, "Port out of range\n");
    usage(argv[0]);
    return -1;
  }

  if(argc == 5) {
    if(strncmp(argv[4], "debug", 5) != 0) {
      usage(argv[0]);
      return -1;
    }
    debug_mode = 1;
  }

  ip = argv[1];
  redirect_url = argv[3];

  location_header = malloc(strlen("Location: \0")+strlen(redirect_url)+1);
  sprintf(location_header, "Location: %s", redirect_url);

  int ret = run(port, ip, redirect_url);

  free(location_header);

  return ret;
  
}
