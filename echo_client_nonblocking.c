#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h> // contains read/write sycalls

#include <netinet/in.h> // needed for sockaddr_in and htonl functions
#include <sys/socket.h> // contains socket, connect, accept syscalls

#include <poll.h> // needed for poll and struct pollfd

#include <fcntl.h> // for fcntl and O_NONBLOCK

enum {
    MAXRCVLEN = 500,
    PORTNUM = 2300
};

int main(int argc, char *argv[])
{
    // struct to store the address of the server we are connecting to
    struct sockaddr_in dest; 

    // zero the struct before filling the fields
    memset(&dest, 0, sizeof(dest));

    // set the type of connection to TCP/IP
    dest.sin_family = AF_INET;

    // set our address to localhost (we are connecting to a local server)
    // htonl is named for Host TO Network Long
    //  -> it reverses the bit order of a long's binary representation
    dest.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    // set the port number to connect to on the server
    // htons is named for Host TO Network Short
    // -> it works the same as htonl but for short (16 bit) numbers
    dest.sin_port = htons(PORTNUM);

    // create a socket file descriptor to connect to the server with
    int mysocket = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);

    //connect to the given address on the given socket file descriptor
    int error = connect(
        mysocket,
        (struct sockaddr *)&dest,
        sizeof(struct sockaddr_in)
    );

    /*if(error != 0) {
        printf("Connection failed\n.");
        exit(1);
    }*/

    // buffer to read messages into (extra byte for null terminator)
    char buffer[MAXRCVLEN + 1];

    // int to store the length of reads and writes
    int len = 0;

    fcntl(0, F_SETFL, O_NONBLOCK);

    struct pollfd fdlist[2];
    fdlist[0].fd = 0;
    fdlist[0].events = POLLIN;
    fdlist[1].fd = mysocket;
    fdlist[1].events = POLLIN;

    while(1) {
        int num_events = poll(fdlist, 2, 10000);
        if(num_events > 0) {
            if(fdlist[0].revents != 0) {
                // zero out the buffer before we read in data
                memset(buffer, 0, MAXRCVLEN + 1);

                // read up to MAXRCVLEN bytes from stdin
                len = read(0, buffer, MAXRCVLEN);

                // skip if an empty line was entered
                if(buffer[0] == '\n') {
                    continue;
                }

                // remove end of line and replace with null terminator
                if(buffer[len - 1] == '\n') {
                    buffer[len - 1] = 0;
                } 

                // send our newly read string to the server via our socket fd
                len = send(mysocket, buffer, len, 0);

                // if zero bytes were read, the connection has closed
                if(len <= 0) {
                    printf("Connection closed by server.\n");
                    break;
                }

                printf("Sent %d bytes: %s\n", len, buffer);

            }
            
            if(fdlist[1].revents != 0) {
                memset(buffer, 0, MAXRCVLEN + 1);

                // read up to MAXRCVLEN bytes from the socket connected to the server
                len = recv(mysocket, buffer, MAXRCVLEN, 0);

                // if zero bytes were written, the connection has closed
                if(len <= 0) {
                    printf("Connection closed by server.\n");
                    break;
                }

                // add a final null terminator in case the server didn't add one
                buffer[len] = 0;
                char* message = buffer;
                int offset = 0;
                while(offset < len) {
                    printf("Received %d bytes: %s\n", strlen(message), message);
                    offset += strlen(message) + 1;
                    message = buffer + offset;
                }
            }
        }
    }

    close(mysocket);
    return EXIT_SUCCESS;
}
