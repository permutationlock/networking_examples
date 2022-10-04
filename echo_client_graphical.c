#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h> // contains read/write sycalls

#include <netinet/in.h> // needed for sockaddr_in and htonl functions
#include <sys/socket.h> // contains socket, connect, accept syscalls

#include <poll.h> // needed for poll and struct pollfd

#include <fcntl.h> // for fcntl and O_NONBLOCK

#include <raylib.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

enum {
    MAXRCVLEN = 500,
    STORELEN = 20,
#ifdef __EMSCRIPTEN__
    PORTNUM = 8082
#else
    PORTNUM = 2300
#endif
};

// socket file descriptor
int mysocket;

// buffer to read messages into (extra byte for null terminator)
char inbuffer[STORELEN][MAXRCVLEN + 1];
int inbuffer_start = 0, inbuffer_end = STORELEN - 1;
char outbuffer[MAXRCVLEN + 1];

// int to store the length of reads and writes
int submit, outlen;

// poll file descriptor list
struct pollfd fdlist[1];

void readKeyboard() {
    int key, len;
    do {
        key = GetKeyPressed();
        if(key >= KEY_A && key <= KEY_Z) {
            outbuffer[outlen] = 'a' + (key - KEY_A);
            outlen += 1;
        }
        if(key == KEY_SPACE) {
            outbuffer[outlen] = ' ';
            outlen += 1;
        }
        if(key == KEY_BACKSPACE) {
            if(outlen > 0) {
                outbuffer[outlen-1] = 0;
                outlen -= 1;
            }
        }
        if(key == KEY_ENTER) {
            submit = 1;
        }
    } while(key > 0);
    
    if(submit > 0) {
        if(outlen > 0) {
            outbuffer[outlen] = '\0';

            // send our newly read string to the server via our socket fd
            len = send(mysocket, outbuffer, outlen, 0);

            // if zero bytes were read, the connection has closed
            if(len <= 0) {
                printf("Connection closed by server.\n");
                exit(0);
            }

            printf("Sent %d bytes: %s\n", len, outbuffer);

            // zero out the buffer before we read in data
            memset(outbuffer, 0, MAXRCVLEN + 1);
        }
        submit = 0;
        outlen = 0;
    }
}

void readSocket() {
    int len;
    int num_events = poll(fdlist, 1, 1);
    char read_buffer[MAXRCVLEN + 1];
    if(num_events > 0) {
        memset(read_buffer, 0, MAXRCVLEN + 1);

        // read up to MAXRCVLEN bytes from the server
        len = recv(mysocket, read_buffer, MAXRCVLEN, 0);

        // if zero bytes were written, the connection has closed
        if(len <= 0) {
            printf("Connection closed by server.\n");
            exit(0);
        }

        // add a final null terminator in case the server didn't add one
        read_buffer[len] = 0;
        char* message = read_buffer;
        int offset = 0;
        while(offset < len) {
            printf("Received %d bytes: %s (%d, %d)\n", strlen(message), message, inbuffer_start, inbuffer_end);
            offset += strlen(message) + 1;
            memset(inbuffer[inbuffer_end], 0, MAXRCVLEN + 1);
            memcpy(inbuffer[inbuffer_end], message, offset - 1);
            inbuffer_end = (inbuffer_end + 1) % STORELEN;
            if(inbuffer_end == inbuffer_start) {
                inbuffer_start = (inbuffer_start + 1) % STORELEN;
            }
            message = read_buffer + offset;
        }
    }
}

void drawFrame() {
    BeginDrawing();
    ClearBackground(RAYWHITE);
    DrawText(
        outbuffer,
        60, 60, 20, DARKGRAY
    );
    int index = 0;
    for(int i = inbuffer_start; i != inbuffer_end; i = (i + 1) % STORELEN) {
        Color c;
        int offset = 10 * index;
        c.r = 235 - offset;
        c.g = 235 - offset;
        c.b = 235 - offset;
        c.a = 255;
        DrawText(
            inbuffer[i],
            320, 60 + 20 * index, 20, c
        );
        index++;
    }
    EndDrawing();
}

void update() {
    readKeyboard();
    readSocket();
    drawFrame();
}

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
    mysocket = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);

    //connect to the given address on the given socket file descriptor
    connect(
        mysocket,
        (struct sockaddr *)&dest,
        sizeof(struct sockaddr_in)
    );

    // setup polling for our socket
    fdlist[0].fd = mysocket;
    fdlist[0].events = POLLIN;

    // create a window to draw in
    InitWindow(640, 480, "raylib [core] example - basic window");
    SetTargetFPS(60);

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(update, 60, 1);
#else
    while (!WindowShouldClose()) {
        update();
    }
#endif

    CloseWindow();
    close(mysocket);
    return EXIT_SUCCESS;
}
