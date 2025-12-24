#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <netinet/in.h>
#include <fcntl.h>

// this is linux specific not available on other OSes like BSD or MacOS
#include <sys/epoll.h>

// will be capped by the OS anyway
#define BACKLOG 32767
#define MAX_EVENTS 32767

int recv_buffer_size=0, send_buffer_size=0, htdocs_dir_size=0;
char *htdocs_dir=NULL;
int mime_n = 13;
int max_ext_size = 7; // a dot + 5 chars + '\0'
char mime_types[][2][32] = {
    {".html", "text/html"},
    {".htm", "text/html"},
    {".txt", "text/plain"},
    {".css", "text/css"},
    {".js", "application/javascript"},
    {".json", "application/json"},
    {".png", "image/png"},
    {".jpg", "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".gif", "image/gif"},
    {".svg", "image/svg+xml"},
    {".ico", "image/x-icon"},
    {".pdf", "application/pdf"},
    {"", "application/octet-stream"} // default
};

char *get_mime_type(char *filename) {
    char *ext = strrchr(filename, '.');
    if (ext != NULL) {
        for (int i = 0; i < mime_n; i++) {
            if (strcmp(ext, mime_types[i][0]) == 0) {
                return mime_types[i][1];
            }
        }
    }
    return mime_types[mime_n][1]; // default
}

int get_listen_socket(int port_num) {
    socklen_t int_size = sizeof(int);
    struct sockaddr_in name;
    /* Give the socket a name. */
    name.sin_family = AF_INET;
    name.sin_port = htons(port_num);
    name.sin_addr.s_addr = htonl (INADDR_ANY);
    int sock = socket (PF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    if (bind(sock, (struct sockaddr *)&name, sizeof(name))) {
        perror("bind");
        exit(EXIT_FAILURE);
    }
    if (listen(sock, BACKLOG)) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    // Get the receive buffer size (SO_RCVBUF)
    if (getsockopt(sock, SOL_SOCKET, SO_RCVBUF, &recv_buffer_size, &int_size)) {
        perror("getsockopt SO_RCVBUF");
        exit(EXIT_FAILURE);
    }
    if (getsockopt(sock, SOL_SOCKET, SO_SNDBUF, &send_buffer_size, &int_size)) {
        perror("getsockopt SO_SNDBUF");
        exit(EXIT_FAILURE);
    }
    fprintf(stderr, "** II ** buf sizes: %d %d\n", recv_buffer_size, send_buffer_size);
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    return sock;
}

#define MAX_VERB_BUF_SIZE 16
#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

void send_400(int conn_sock) {
    const char *response =
        "HTTP/1.1 400 Bad Request\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 11\r\n"
        "\r\n"
        "Bad Request";
    send(conn_sock, response, strlen(response), 0);
    close(conn_sock);
}

long fd_size(int fd) {
    struct stat statbuf;
    if (fstat(fd, &statbuf)) {
        perror("fstat");
        return -1;
    }
    return statbuf.st_size;
}

void handle_one(int conn_sock) {
    char buf[recv_buffer_size];
    char uri[recv_buffer_size];
    char verb[MAX_VERB_BUF_SIZE];
    verb[0]='\0';
    uri[0]='\0';
    char *ptr;
    int line_size = 0;
    int verb_size = 0;
    int uri_size = 0;
    int drain=0;
    ssize_t bytes_received;
    // drain all incoming data
    while ((bytes_received = recv(conn_sock, buf, sizeof(buf), 0)) > 0) {
        // TODO: in the future we need to parse headers for Range:, public caching, tags, if not modified, etc.
        // https://developer.mozilla.org/en-US/docs/Web/HTTP/Guides/Range_requests#single_part_ranges
        if (drain) continue;
        drain=1;
        ptr = memchr(buf, '\n', bytes_received);
        if (ptr == NULL) continue;
        line_size = ptr-buf;
        // parse verb
        ptr = memchr(buf, ' ', MIN(line_size, MAX_VERB_BUF_SIZE));
        if (ptr == NULL) continue;
        verb_size = ptr - buf;
        memcpy(verb, buf, verb_size);
        verb[verb_size] = '\0';
        // parse uri
        char *uri_start = ptr + 1;
        ptr = memchr(uri_start, ' ', line_size - verb_size - 1);
        if (ptr == NULL) continue;
        uri_size = ptr - uri_start;
        memcpy(uri, uri_start, uri_size);
        uri[uri_size] = '\0';
        // fprintf(stderr, "** II ** request line: verb=[%s] uri=[%s]\n", verb, uri);
    }
    if (bytes_received == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
        perror("recv");
        close(conn_sock);
        return;
    }
    // TODO: handle HEAD and OPTIONS.
    if (verb_size==0 || uri_size==0 || uri[0]!='/' || strcmp(verb, "GET")!=0) {
        send_400(conn_sock);
        return;
    }
    if (strstr(uri, "..") != NULL) {
        // prevent directory traversal attacks
        send_400(conn_sock);
        return;
    }
    // construct full pathname
    char pathname_size = htdocs_dir_size + uri_size + 1;
    char pathname[pathname_size];
    memcpy(pathname, htdocs_dir, htdocs_dir_size);
    // uri already contains the leading '/'
    memcpy(pathname + htdocs_dir_size, uri, uri_size);
    pathname[htdocs_dir_size + uri_size] = '\0';
    // fprintf(stderr, "** II ** full path: [%s]\n", pathname);
    int fd = open(pathname, O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "** WW ** could not open: [%s]\n", pathname);
        perror("open");
        send_400(conn_sock);
        return;
    }
    long file_size = fd_size(fd);
    if (file_size == -1) {
        close(fd);
        send_400(conn_sock);
        return;
    }
    // hack to make guessing mime faster by sending only the last few chars of the path
    char *path_part = pathname + MAX(0, pathname_size - max_ext_size);
    // fprintf(stderr, "** II ** path_part=[%s]\n", path_part);
    char *mime_type = get_mime_type(path_part);
    const char *template =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %d\r\n"
        "\r\n";
    char dst[send_buffer_size];
    int size = snprintf(dst, send_buffer_size, template, mime_type, file_size);
    if (size<0 || size>=send_buffer_size) {
        fprintf(stderr, "** EE ** response header too large\n");
        close(fd);
        send_400(conn_sock);
        return;
    }
    send(conn_sock, dst, size, 0);
    sendfile(conn_sock, fd, NULL, file_size);
    close(fd);
    close(conn_sock);
}

void serve_forever(int port_num) {
    struct epoll_event ev, events[MAX_EVENTS];
    fprintf(stderr, "** II ** serving [%s] on port %d\n", htdocs_dir, port_num);
    int listen_sock=get_listen_socket(port_num);
    int conn_sock, nfds, epollfd;
    epollfd = epoll_create1(0);
    if (epollfd == -1) {
        perror("epoll_create1");
        exit(EXIT_FAILURE);
    }
    ev.events = EPOLLIN;
    ev.data.fd = listen_sock;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, listen_sock, &ev) < 0) {
        perror("epoll_ctl: listen_sock");
        exit(EXIT_FAILURE);
    }
    int n;
    socklen_t addrlen;
    struct sockaddr_in  addr;
    for (;;) {
        nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
        if (nfds<0) {
            if (errno==EINTR) continue;
            perror("epoll_wait");
            exit(EXIT_FAILURE);
        }
        for (n = 0; n < nfds; ++n) {
            if (events[n].data.fd != listen_sock) {
                /*
                consume all of the incoming data in the request.
                In Edge-Triggered mode, you cannot be "lazy." You must consume all available data (read until EAGAIN) and ensure all outgoing data is accepted by the kernel before moving on
                */
                handle_one(events[n].data.fd);
                continue;
            }
            while((conn_sock=accept4(listen_sock, (struct sockaddr *)&addr, &addrlen, SOCK_NONBLOCK))>0) {
                // fprintf(stderr, "** II ** accepted %d\n", conn_sock);
                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = conn_sock;
                if (epoll_ctl(epollfd, EPOLL_CTL_ADD, conn_sock, &ev)<0) {
                    perror("epoll_ctl: conn_sock");
                    exit(EXIT_FAILURE);
                }
            }
            if (conn_sock<0 && errno!=EAGAIN && errno!=EWOULDBLOCK) {
                perror("accept");
                exit(EXIT_FAILURE);
            }
        }
    }
}

int main(int argc, char *argv[]) {
    int port_num = 8080; // Default port number
    char c;
    while ((c = getopt(argc, argv, "d:p:")) != -1) {
        switch (c) {
            case 'd':
                // The argument for -d is stored in the global variable optarg
                htdocs_dir = optarg;
                break;
            case 'p':
                // The argument for -p is stored in the global variable optarg
                port_num = atoi(optarg);
                break;
            case '?': // Handle unknown or missing options/arguments
                fprintf(stderr, "Usage: %s [-d htdocs_dir] [-p port_num]\n", argv[0]);
                return 1; // Exit on error
            default:
                abort();
        }
    }
    if (htdocs_dir == NULL) {
        htdocs_dir = "./";
    }
    htdocs_dir_size = strlen(htdocs_dir);
    serve_forever(port_num);
    return 0;
}