#include <algorithm>
#include <arpa/inet.h>
#include <errno.h>
#include <mutex>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <vector>

void usage() {
    printf("syntax : echo-server <port> [-e[-b]]\n");
    printf("sample : echo-server 1234 -e -b\n");
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
    bool echo{false};
    bool broadcast{false};
    uint16_t port{0};

    bool parse(int argc, char* argv[]) {
        if (argc < 2) return false;

        int p = atoi(argv[1]);
        if (p <= 0 || p > 65535) return false;
        port = static_cast<uint16_t>(p);

        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "-e") == 0) {
                echo = true;
                continue;
            }
            if (strcmp(argv[i], "-b") == 0) {
                broadcast = true;
                continue;
            }
            return false;
        }

        return true;
    }
} param;

std::vector<int> clientList;
std::mutex clientMutex;

void addClient(int sd) {
    std::lock_guard<std::mutex> lock(clientMutex);
    clientList.push_back(sd);
}

void removeClient(int sd) {
    std::lock_guard<std::mutex> lock(clientMutex);
    clientList.erase(std::remove(clientList.begin(), clientList.end(), sd), clientList.end());
}

void sendToOne(int sd, const char* buf, ssize_t len) {
    if (!sendData(sd, buf, len)) {
        shutdown(sd, SHUT_RDWR);
    }
}

void sendToAll(const char* buf, ssize_t len) {
    std::lock_guard<std::mutex> lock(clientMutex);

    for (int clientSd : clientList) {
        if (!sendData(clientSd, buf, len)) {
            shutdown(clientSd, SHUT_RDWR);
        }
    }
}

void recvThread(int sd) {
    static const int BUFSIZE = 65536;
    char buf[BUFSIZE];

    printf("client connected\n");
    fflush(stdout);

    while (true) {
        ssize_t res = ::recv(sd, buf, BUFSIZE - 1, 0);
        if (res == 0) {
            fprintf(stderr, "client closed connection\n");
            break;
        }
        if (res == -1) {
            myerror("recv");
            break;
        }

        buf[res] = '\0';
        printf("%s", buf);
        fflush(stdout);

        if (param.broadcast) {
            sendToAll(buf, res);
        } else if (param.echo) {
            sendToOne(sd, buf, res);
        }
    }

    removeClient(sd);
    ::close(sd);
    printf("client disconnected\n");
    fflush(stdout);
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

    int optval = 1;
    int res = ::setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    if (res == -1) {
        myerror("setsockopt");
        ::close(sd);
        return -1;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(param.port);
    memset(&addr.sin_zero, 0, sizeof(addr.sin_zero));

    res = ::bind(sd, (struct sockaddr*)&addr, sizeof(addr));
    if (res == -1) {
        myerror("bind");
        ::close(sd);
        return -1;
    }

    res = ::listen(sd, 5);
    if (res == -1) {
        myerror("listen");
        ::close(sd);
        return -1;
    }

    while (true) {
        struct sockaddr_in clientAddr;
        socklen_t clientAddrLen = sizeof(clientAddr);

        int clientSd = ::accept(sd, (struct sockaddr*)&clientAddr, &clientAddrLen);
        if (clientSd == -1) {
            myerror("accept");
            break;
        }

        addClient(clientSd);
        std::thread t(recvThread, clientSd);
        t.detach();
    }

    ::close(sd);
    return 0;
}

