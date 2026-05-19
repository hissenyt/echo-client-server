#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>

void usage() {
    printf("syntax : echo-client <ip> <port>\n");
    printf("sample : echo-client 192.168.10.2 1234\n");
}

void myerror(const char* msg) {
    fprintf(stderr, "%s %s %d\n", msg, strerror(errno), errno);
}

bool sendData(int sd, const char* buf, ssize_t len) {
    ssize_t sent = 0;

    while (sent < len) {
        ssize_t res = ::send(sd, buf + sent, len - sent, 0);
        if (res <= 0) return false;
        sent += res;
    }

    return true;
}

struct Param {
    struct in_addr ip;
    uint16_t port{0};

    bool parse(int argc, char* argv[]) {
        if (argc != 3) return false;

        int res = inet_pton(AF_INET, argv[1], &ip);
        if (res == 0) {
            fprintf(stderr, "invalid ip address\n");
            return false;
        }
        if (res == -1) {
            myerror("inet_pton");
            return false;
        }

        int p = atoi(argv[2]);
        if (p <= 0 || p > 65535) return false;

        port = static_cast<uint16_t>(p);
        return true;
    }
} param;

void recvThread(int sd) {
    static const int BUFSIZE = 65536;
    char buf[BUFSIZE];

    while (true) {
        ssize_t res = ::recv(sd, buf, BUFSIZE - 1, 0);
        if (res == 0) {
            fprintf(stderr, "server closed connection\n");
            break;
        }
        if (res == -1) {
            myerror("recv");
            break;
        }

        buf[res] = '\0';
        printf("%s", buf);
        fflush(stdout);
    }

    ::close(sd);
    exit(0);
}

int main(int argc, char* argv[]) {
    if (!param.parse(argc, argv)) {
        usage();
        return -1;
    }

    signal(SIGPIPE, SIG_IGN);

    int sd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sd == -1) {
        myerror("socket");
        return -1;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(param.port);
    addr.sin_addr = param.ip;
    memset(&addr.sin_zero, 0, sizeof(addr.sin_zero));

    int res = ::connect(sd, (struct sockaddr*)&addr, sizeof(addr));
    if (res == -1) {
        myerror("connect");
        ::close(sd);
        return -1;
    }

    std::thread t(recvThread, sd);
    t.detach();

    static const int BUFSIZE = 65536;
    char buf[BUFSIZE];

    while (fgets(buf, BUFSIZE, stdin) != nullptr) {
        ssize_t len = strlen(buf);
        if (len == 0) continue;

        if (!sendData(sd, buf, len)) {
            myerror("send");
            break;
        }
    }

    ::close(sd);
    return 0;
}

