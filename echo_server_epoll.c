#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h> // contains close sycall

#include <sys/socket.h> // contains socket, connect, accept syscalls
#include <netinet/in.h> // needed for sockaddr_in and htonl functions
#include <arpa/inet.h> // needed for inet_ntoa

#include <sys/epoll.h> // needed for poll syscall and struct pollfd

#include <errno.h> // needed for errno global

enum {
  PORTNUM = 2300,
  MAXCONS = 5,
  MAXRECVLEN = 500,
  NUMEVENTS = 20
};

int epollfd;

int handle_connection(int confd) {
    int len = 0;
    char buffer[MAXRECVLEN+1];

    // read a new message from the socket
    len = recv(confd, buffer, MAXRECVLEN, 0);
   
    if(len < 0) {
        // if the socket would block, quit without error
        if(errno == EAGAIN || errno == EWOULDBLOCK) return 1;
        return 0;
    } if(len == 0) {
        return 0;
    }

    buffer[len] = '\0';
    printf("client %d sent: %s\n", confd, buffer);

    // send the message back to the client
    len = send(confd, buffer, len, 0);

    if(len <= 0) {
        return 0;
    }

    return 1;
}

int accept_connection(int socketfd) {
    // stores size of address (will be set to size of connection address)
    socklen_t socksize = sizeof(struct sockaddr_in);

    // socket info about the machine connecting to us
    struct sockaddr_in dest;

    // accept a single connection on the socket fd mysocket
    int confd = accept4(
        socketfd,
        (struct sockaddr *)&dest,
        &socksize,
        SOCK_NONBLOCK
    );

    printf("Incoming connection from %s assigned to fd %d\n",
        inet_ntoa(dest.sin_addr), confd);


    // register our connection for epoll POLLIN events
    struct epoll_event ev = { 0 };
    ev.data.fd = confd;
    ev.events = EPOLLIN;

    return epoll_ctl(epollfd, EPOLL_CTL_ADD, confd, &ev);
}

// clean up data and close client connection
int clean_connection(int confd) {
    printf("closing connection %d\n", confd);
    close(confd);
    return epoll_ctl(epollfd, EPOLL_CTL_DEL, confd, NULL);
}

int main(int argc, char *argv[])
{
    // initialize array to blank memory
    epollfd = epoll_create(0xf00d);

    // struct to store the address and socket of our server
    struct sockaddr_in serv;

    // zero the struct before filling the fields
    memset(&serv, 0, sizeof(serv));

    // set the type of connection to TCP/IP
    serv.sin_family = AF_INET;                

    // set our address to accept connections on any interface
    // htonl is named for Host TO Network Long
    //  -> it reverses the bit order of a long's binary representation
    serv.sin_addr.s_addr = htonl(INADDR_ANY); 

    // set the server port number
    // htons is named for Host TO Network Short
    // -> it works the same as htonl but for short (16 bit) numbers
    serv.sin_port = htons(PORTNUM);

    // non-blocking socket used to listen for incoming connections
    int mysocket = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);

    // set socket flag to allow address reuse
    int sopt = 1;
    setsockopt(
        mysocket, SOL_SOCKET, SO_REUSEADDR, (char *)&sopt, sizeof(sopt)
    );

    // bind serv information to mysocket
    bind(mysocket, (struct sockaddr *)&serv, sizeof(struct sockaddr_in));

    // start listening, allowing up to MAXCONS connections
    listen(mysocket, MAXCONS);

    {
        struct epoll_event ev = { 0 };
        ev.data.fd = mysocket;
        ev.events = EPOLLIN;

        epoll_ctl(epollfd, EPOLL_CTL_ADD, mysocket, &ev);
    }

    int num_events;
    struct epoll_event pevents[NUMEVENTS];
    while(1) {
        // wait for any of the following events:
        //   a new client connection is opened
        //   an open connection sends a message
        //   a previously blocked socket becomes writeable
        num_events = epoll_wait(epollfd, pevents, NUMEVENTS, 10000);

        // kill program if an error occurs (should never happen)
        if(num_events < 0) exit(1);

        // handle any new events
        for(int i=0; i < num_events; i++) {
            if(pevents[i].events & EPOLLIN) {
                int fd = pevents[i].data.fd;
                // accept any new connections
                if(fd == mysocket) {
                    accept_connection(mysocket);
                } else {
                    if(handle_connection(fd) < 1) {
                        clean_connection(fd);
                    }
                }
            }
        }
    }
    
    return EXIT_SUCCESS;
}
