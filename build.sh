rm -Rf build
mkdir build
gcc -o build/client echo_client_nonblocking.c
gcc -o build/server echo_server_epoll.c
gcc -o build/gl_client -lraylib echo_client_graphical.c
