#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#define PORT "13112"

#define BACKLOG 10

#define MAXDATASIZE 100

void sigchld_handler(int s)
{
	(void)s;

	int saved_errno = errno;

	while(waitpid(-1, NULL, WNOHANG) > 0);

	errno = saved_errno;
}

void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


int main () {
    printf("Do you want to host a chat or connect to one?\n1 - Host\n2 - Connect\n");
    int choice=0;
    while (choice!=1 && choice!=2)
    {
        scanf("%d", &choice);
        if(choice!=1 && choice!=2) {
            printf("Invalid input.\n");
            scanf("%d", &choice);
        }
    }
    if(choice == 1) {
        int listsock_fd, commsock_fd;
        struct addrinfo hints, *servinfo, *p;
        struct sockaddr_storage client_addr;
        socklen_t sin_size;
	    struct sigaction sa;
        int yes = 1;
        char s[INET6_ADDRSTRLEN];

        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;

        int rv;
        if((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
            fprintf(stderr, "getaddrinfo: %s", gai_strerror(rv));
            return 1;
        }

        for(p=servinfo; p!=NULL;p=p->ai_next) {
            if ((listsock_fd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("chat-server: socket");
			continue;
            }

            if (setsockopt(listsock_fd, SOL_SOCKET, SO_REUSEADDR, &yes,
                    sizeof(int)) == -1) {
                perror("setsockopt");
                exit(1);
            }

            if (bind(listsock_fd, p->ai_addr, p->ai_addrlen) == -1) {
                close(listsock_fd);
                perror("chat-server: bind");
                continue;
            }

            break;
        }
        
        freeaddrinfo(servinfo);

        if(p == NULL) {
            fprintf(stderr, "chat-server: failed to bind\n");
		    exit(1);
        }

        if (listen(listsock_fd, BACKLOG) == -1) {
            perror("listen");
            exit(1);
	    }

        //TODO: figure this out lol
        sa.sa_handler = sigchld_handler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_RESTART;
        if (sigaction(SIGCHLD, &sa, NULL) == -1) {
            perror("sigaction");
            exit(1);
        }

        printf("chat-server: waiting for connections...\n");

        sin_size = sizeof client_addr;
        commsock_fd = accept(listsock_fd, (struct sockaddr *)&client_addr, &sin_size);
        if (commsock_fd == -1) {
            perror("accept");
            return 1;
        }

        inet_ntop(client_addr.ss_family,
            get_in_addr((struct sockaddr *)&client_addr),
            s, sizeof s);
        printf("chat-server: got connection from %s\n", s);\
        char msg[MAXDATASIZE];
        while (strcmp(msg,"/exit") != 0)
        {
            printf("Type your message: ");
            scanf(" %s", msg);
            if (!fork()) { 
                close(listsock_fd);   
                if (send(commsock_fd, msg, MAXDATASIZE-1, 0) == -1)
                    perror("send");
                close(commsock_fd);
                exit(0);
            }
        }
        close(commsock_fd); 
    }
    else {
        int consock_fd, numbytes;  
        char buf[MAXDATASIZE];
        struct addrinfo hints, *servinfo, *p;
        int rv;
        char s[INET6_ADDRSTRLEN];

        char inputaddr[INET6_ADDRSTRLEN];
        scanf("%s", inputaddr);

        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        if ((rv = getaddrinfo(inputaddr, PORT, &hints, &servinfo)) != 0) {
            fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
            return 1;
        }

        for(p = servinfo; p != NULL; p = p->ai_next) {
            if ((consock_fd = socket(p->ai_family, p->ai_socktype,
                    p->ai_protocol)) == -1) {
                perror("chat-client: socket");
                continue;
            }

            if (connect(consock_fd, p->ai_addr, p->ai_addrlen) == -1) {
                perror("chat-client: connect");
                close(consock_fd);
                continue;
            }

            break;
        }

        if (p == NULL) {
            fprintf(stderr, "chat-client: failed to connect\n");
            return 2;
        }

        inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
                s, sizeof s);
        printf("chat-client: connecting to %s\n", s);

        freeaddrinfo(servinfo);

        while(1) {
            if ((numbytes = recv(consock_fd, buf, MAXDATASIZE-1, 0)) == -1) {
                perror("recv");
                exit(1);
            }

            buf[numbytes] = '\0';

            printf("Received message '%s'\n",buf);

        }
        close(consock_fd);
    }
    return 0;   
    
}