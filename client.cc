#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <stdlib.h>
#include <unistd.h>

#include <iostream>
#include <unordered_map>
#include <fstream>

using namespace std;

static int id;

int dispatch(std::string cmd) {
    std::unordered_map<string, string> tb {
        {"quit", "Q"},
        {"pause", "P"},
        {"resume", "R"},
    };

    if (tb.find(cmd) == tb.end()) return -1;

    if (write(id, tb[cmd].c_str(), tb[cmd].size()) < 0) {
        perror("write");
        return -1;
    }
    std::cerr << "write " << tb[cmd].size() << " bytes" << std::endl;
    return 0;
}

int main(int argc, char *argv[])
{
    const char *sock_path = ":prometheus.sock";

    if (argc < 2) {
        std::cerr << "no command specified" << std::endl;
        return 0;
    }

    id = socket(AF_UNIX, SOCK_STREAM, 0);
    if (id < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_un un2;
    memset(&un2, 0, sizeof un2);
    un2.sun_family = AF_UNIX;
    strncpy(&un2.sun_path[1], sock_path, strlen(sock_path));
    if (connect(id, (struct sockaddr*)&un2, sizeof un2) < 0) {
        perror("connect");
        close(id);
        return -1;
    }

    dispatch(argv[1]);
    close(id);

    ofstream ofs{"/tmp/prometheus.done"};
    ofs << "";

    // bad way to wait for server to finish
    sleep(1);
    return 0;
}

