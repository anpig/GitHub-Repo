#include <iostream>
#include <sstream>
#include <vector>
#include <map>
#include <queue>
#include <string>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/unistd.h>
#include <sys/select.h>
#include <netdb.h>
#include <errno.h>
#include <bits/typesizes.h>

#define	LISTENQ	1024
#define MAXLINE 4096
#define MAXCLI 1024

#define WRONG_USAGE 1
#define WRONG_PASSWORD 2
#define WRONG_STATUS 3
#define WRONG_PORT 4
#define WRONG_VERSION 5
#define EXISTANCE 6
#define BLACK_LISTED 7

using namespace std;

vector<string> filter_list = {"how", "you", "or", "pek0", "tea", "ha", "kon", "pain", "Starburst Stream"};
string base64table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
map<char, int> base64reverse;

string filter(string context) {
    for (string s : filter_list) {
        size_t pos = context.find(s);
        while (pos != string::npos) {
            context.replace(context.find(s), s.size(), string(s.size(), '*'));
            pos = context.find(s, pos + 1);
        }
    }
    return context;
}

string tobase64(string input) {
    int leng = input.length();
    int tail = leng * 4 % 3;
    string output;
    int i;
    for (i = 0; i < leng - 3; i += 3) {
        output += base64table[input[i] >> 2];
        output += base64table[((input[i] & 0x03) << 4) + (input[i + 1] >> 4)];
        output += base64table[((input[i + 1] & 0x0F) << 2) + (input[i + 2] >> 6)];
        output += base64table[input[i + 2] & 0x3F];
    }
    output += base64table[input[i] >> 2];
    if (tail == 0) {
        output += base64table[((input[i] & 0x03) << 4) + (input[i + 1] >> 4)];
        output += base64table[((input[i + 1] & 0x0F) << 2) + (input[i + 2] >> 6)];
        output += base64table[input[i + 2] & 0x3F];
    }
    else if (tail == 1) {
        output += base64table[(input[i] & 0x03) << 4];
        output += "==";
    }
    else if (tail == 2) {
        output += base64table[((input[i] & 0x03) << 4) + (input[i + 1] >> 4)];
        output += base64table[(input[i + 1] & 0x0F) << 2];
        output += '=';
    }
    return output;
}

string frombase64(string input) {
    string output;
    int tmp = 0, bitcount = 0, leng = input.length();
    for (int i = 0; i < leng; i++) {
        if (input[i] == '=' || input[i] == 0) break;
        tmp = (tmp << 6) + base64reverse[input[i]];
        bitcount += 6;
        if (bitcount >= 8) {
            bitcount -= 8;
            output += (char)(tmp >> bitcount);
        } 
    }
    return output;
}

struct packet_header {
    unsigned char flag;
    unsigned char version;
    unsigned char payload[0];
} __attribute__((packed));

struct packet_data {
    unsigned short len;
    unsigned char data[0];
} __attribute__((packed));

typedef struct user{
    int violated = 0, protocol = 0, port = 0;
    string username;
    string password;
    user() {}
    user(string a, string b) {
        username = a;
        password = b;
    }
} user;

int main(int argc, char **argv)
{
    if (argc != 2) {
        cout << "usage: hw3 [port]" << endl;
        return ENOEXEC;
    }

	/* initialize sockets */
    int i, cli_cnt, maxfd, listenfd, connfd, sockfd, port_int, udpfd;
    int	nready, client[MAXCLI];
    fd_set rset, allset;
    char in_buff[MAXLINE], out_buff[MAXLINE], out_ver1[MAXLINE], out_ver2[MAXLINE];
    socklen_t clilen;
    struct sockaddr_in cliaddr, servaddr;
    stringstream ss;
    ss << argv[1];
    ss >> port_int;
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    udpfd = socket(AF_INET, SOCK_DGRAM, 0);
    int enable = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port        = htons(port_int);

    bind(listenfd, (sockaddr *) &servaddr, sizeof(servaddr));
    bind(udpfd, (sockaddr *) &servaddr, sizeof(servaddr));

    listen(listenfd, LISTENQ);

    maxfd = listenfd;			/* initialize */
    cli_cnt = -1;					/* index into client[] array */
    for (i = 0; i < MAXCLI; i++) {
        client[i] = -1;			/* -1 indicates available entry */
    }
    FD_ZERO(&allset);
    FD_SET(listenfd, &allset);
    FD_SET(udpfd, &allset);

    /* welcome and prompt */ 
    char welcomemessage[MAXLINE] = "********************************\n** Welcome to the BBS server. **\n********************************\n% ";
    char prompt[3] = "% ";

    /* initialize app */
    vector<bool> logged(MAXCLI);
    vector<string> logged_user(MAXCLI);
    vector<pair<int, int> > udpcli;
    map<string, int> userindex;
    map<string, bool> online;
    vector<user> users;
    string chat_history;
    users.emplace_back(user());
    for (int i = 0; i < 64; i++) base64reverse[base64table[i]] = i;

    while (true) {
        /* structure assignment */
        rset = allset;
        nready = select(maxfd + 1, &rset, NULL, NULL, NULL);

        /* new client connection */
        if (FD_ISSET(listenfd, &rset)) {
			clilen = sizeof(cliaddr);
			connfd = accept(listenfd, (sockaddr *) &cliaddr, &clilen);
			for (i = 0; i < MAXCLI; i++) {
				if (client[i] < 0) {
					// save descriptor
					client[i] = connfd;
                    logged[i] = false;
                    online[logged_user[i]] = false;
                    logged_user[i] = "";
                    write(client[i], welcomemessage, strlen(welcomemessage));
					break;
				}
            }
			// if (i == MAXCLI) err_quit("too many clients");

			// add new descriptor to set
			FD_SET(connfd, &allset);		
			
			// for select
			if (connfd > maxfd) maxfd = connfd;

			// max index in client[] array
			if (i >= cli_cnt) cli_cnt = i + 1;

			// no more readable descriptors
			if (--nready <= 0) continue;
		}
        
        /* check all clients for data */
        for (i = 0; i < cli_cnt; i++) {
			if (client[i] < 0) continue;
            sockfd = client[i];
			if (FD_ISSET(sockfd, &rset)) {
                if (logged_user[i] != "" && users[userindex[logged_user[i]]].violated >= 3 ) {
                    snprintf(out_buff, sizeof(out_buff), "Bye, %s.\n", logged_user[i].c_str());
                    logged[i] = false;
                    online[logged_user[i]] = false;
                    logged_user[i] = "";
                    for (auto it = udpcli.begin(); it != udpcli.end(); it++) {
                        if (it->first == users[userindex[logged_user[i]]].port && it->second == users[userindex[logged_user[i]]].protocol) {
                            udpcli.erase(it);
                            break;
                        }
                    }
                    write(sockfd, out_buff, strlen(out_buff));
                    write(sockfd, prompt, strlen(prompt));
                }
				if (read(sockfd, in_buff, MAXLINE) == 0) {
					// connection closed by client
					close(sockfd);
					FD_CLR(sockfd, &allset);
					client[i] = -1;
				}
                else {
                    /* assign something */ 
                    stringstream ss;
                    bool exited = false;
                    for (int charpointer = 0; charpointer < MAXLINE; charpointer++) {
                        if (exited) break;
                        if (in_buff[charpointer] == '\0') break;
                        else if (in_buff[charpointer] == '\n') {
                            /* input func */
                            bzero(out_buff, MAXLINE);
                            string func;
                            ss >> func;
                            if (func == "register") {
                                int error = 0;
                                string username, password;
                                if (ss >> username) {
                                    if (!(ss >> password)) error = WRONG_USAGE;
                                    else if (userindex[username]) error = EXISTANCE;
                                }
                                else error = WRONG_USAGE;
                                switch (error) {
                                    case WRONG_USAGE:
                                        snprintf(out_buff, sizeof(out_buff), "Usage: register <username> <password>\n");
                                        break;
                                    case EXISTANCE:
                                        snprintf(out_buff, sizeof(out_buff), "Username is already used.\n");
                                        break;
                                    default:
                                        userindex[username] = users.size();
                                        users.emplace_back(user(username, password));
                                        cout << userindex[username] << ' ' << username << endl;
                                        snprintf(out_buff, sizeof(out_buff), "Register successfully.\n");
                                }
                            }
                            else if (func == "login") {
                                int error = 0;
                                string username, password, trash;
                                if (ss >> username) {
                                    if (ss >> password) {  
                                        if (ss >> trash) error = WRONG_USAGE;
                                        else if (logged[i] || online[username]) error = WRONG_STATUS;
                                        else if (!userindex[username]) error = EXISTANCE;
                                        else if (users[userindex[username]].violated >= 3) error = BLACK_LISTED;
                                        else if (password != users[userindex[username]].password) error = WRONG_PASSWORD;
                                    }
                                    else error = WRONG_USAGE;
                                }
                                else error = WRONG_USAGE;
                                switch(error) {
                                    case WRONG_USAGE:
                                        snprintf(out_buff, sizeof(out_buff), "Usage: login <username> <password>\n");
                                        break;
                                    case EXISTANCE:
                                        snprintf(out_buff, sizeof(out_buff), "Login failed.\n");
                                        break;
                                    case BLACK_LISTED:
                                        snprintf(out_buff, sizeof(out_buff), "We don't welcome %s!\n", username.c_str());
                                        break;
                                    case WRONG_PASSWORD:
                                        snprintf(out_buff, sizeof(out_buff), "Login failed.\n");
                                        break;
                                    case WRONG_STATUS:
                                        snprintf(out_buff, sizeof(out_buff), "Please logout first.\n");
                                        break;
                                    default:
                                        logged[i] = true;
                                        logged_user[i] = username;
                                        online[username] = true;
                                        snprintf(out_buff, sizeof(out_buff), "Welcome, %s.\n", logged_user[i].c_str());
                                }
                            }
                            else if (func == "logout") {
                                string trash;
                                if (ss >> trash) snprintf(out_buff, sizeof(out_buff), "Usage: logout\n");
                                else if (!logged[i]) snprintf(out_buff, sizeof(out_buff), "Please login first.\n");
                                else {
                                    snprintf(out_buff, sizeof(out_buff), "Bye, %s.\n", logged_user[i].c_str());
                                    for (auto it = udpcli.begin(); it != udpcli.end(); it++) {
                                        if (it->first == users[userindex[logged_user[i]]].port && it->second == users[userindex[logged_user[i]]].protocol) {
                                            udpcli.erase(it);
                                            break;
                                        }
                                    }
                                    logged[i] = false;
                                    online[logged_user[i]] = false;
                                    logged_user[i] = "";
                                }
                            }
                            else if (func == "enter-chat-room") {
                                int error = 0, port, version;
                                string portss, versionss, trash;
                                stringstream fuck, shit;
                                if (ss >> portss) {
                                    fuck << portss;
                                    fuck >> port;
                                    if (ss >> versionss) {
                                        shit << versionss;
                                        shit >> version;
                                        if (ss >> trash) error = WRONG_USAGE;
                                        if (port < 0 || port > 65535) error = WRONG_PORT;
                                        else if (version != 1 && version != 2) error = WRONG_VERSION;
                                        else if (!logged[i]) error = WRONG_STATUS;
                                    }
                                    else error = WRONG_USAGE;
                                }
                                else error = WRONG_USAGE;
                                switch(error) {
                                    case WRONG_USAGE:
                                        snprintf(out_buff, sizeof(out_buff), "Usage: enter-chat-room <port> <version>\n");
                                        break;
                                    case WRONG_PORT:
                                        snprintf(out_buff, sizeof(out_buff), "Port %s is not valid.\n", portss.c_str());
                                        break;
                                    case WRONG_VERSION:
                                        snprintf(out_buff, sizeof(out_buff), "Version %s is not supported.\n", versionss.c_str());
                                        break;
                                    case WRONG_STATUS:
                                        snprintf(out_buff, sizeof(out_buff), "Please login first.\n");
                                        break;
                                    default:
                                        users[userindex[logged_user[i]]].port = port;
                                        users[userindex[logged_user[i]]].protocol = version;
                                        udpcli.emplace_back(make_pair(port, version));
                                        snprintf(out_buff, sizeof(out_buff), "Welcome to public chat room.\nPort:%d\nVersion:%d\n%s", port, version, chat_history.c_str());
                                }
                            }
                            else if (func == "exit") {
                                string trash;
                                if (ss >> trash) {
                                    snprintf(out_buff, sizeof(out_buff), "Usage: exit\n");
                                }
                                else { 
                                    if (logged[i]) {
                                        for (auto it = udpcli.begin(); it != udpcli.end(); it++) {
                                            if (it->first == users[userindex[logged_user[i]]].port && it->second == users[userindex[logged_user[i]]].protocol) {
                                                udpcli.erase(it);
                                                break;
                                            }
                                        }
                                        snprintf(out_buff, sizeof(out_buff), "Bye, %s.\n", logged_user[i].c_str());
                                        write(sockfd, out_buff, strlen(out_buff));
                                    }
                                    logged[i] = false;
                                    logged_user[i] = "";
                                    exited = true;
                                    FD_CLR(sockfd, &allset);
                                    client[i] = -1;
                                    close(sockfd);
                                    break;
                                }
                            }
                            write(sockfd, out_buff, strlen(out_buff));
                            ss.str("");
                            ss.clear();
                            write(sockfd, prompt, strlen(prompt));
                        }
                        else ss << in_buff[charpointer];
                    }
                    bzero(in_buff, MAXLINE);
                }

                // no more readable descriptors
                if (--nready <= 0) break;
            }
        }
        if (FD_ISSET(udpfd, &rset)) {
            size_t in_leng = recvfrom(udpfd, in_buff, MAXLINE, 0, (sockaddr *) &cliaddr, &clilen);
            int ver = (int)in_buff[1];
            string name, message, filtered, base64name, base64filtered;
            bzero(out_ver1, MAXLINE);
            bzero(out_ver2, MAXLINE);
            switch (ver) {
                case 1: {
                    short leng_name = (in_buff[2] << 8) + in_buff[3];
                    int ii;
                    for (ii = 4; ii < leng_name + 4; ii++) name += in_buff[ii];
                    short leng_message = (in_buff[ii++] << 8);
                    leng_message += in_buff[ii++];
                    int offset = ii;
                    for (; ii < leng_message + offset; ii++) message += in_buff[ii];
                    filtered = filter(message);
                    if (filtered != message) users[userindex[name]].violated++;
                    chat_history += name + ':' + filtered + '\n';
                    if (users[userindex[name]].violated >= 3) {
                        for (auto it = udpcli.begin(); it != udpcli.end(); it++) {
                            if (it->first == ntohs(cliaddr.sin_port) && it->second == 1) {
                                udpcli.erase(it);
                                break;
                            }
                        }
                    }
                    base64name = tobase64(name);
                    base64filtered = tobase64(filtered);
                    struct packet_header *header = (struct packet_header*) out_ver1;
                    struct packet_data *data_name = (struct packet_data*) (out_ver1 + sizeof(struct packet_header));
                    struct packet_data *data_msg = (struct packet_data*) (out_ver1 + sizeof(struct packet_header) + sizeof(struct packet_data) + name.size());
                    header->flag = 0x01;
                    header->version = 0x01;
                    data_name->len = htons(name.size());
                    memcpy(data_name->data, name.c_str(), name.size());
                    data_msg->len = htons(filtered.size());
                    memcpy(data_msg->data, filtered.c_str(), filtered.size());
                    sprintf(out_ver2, "\x01\x02%s\n%s\n", base64name.c_str(), base64filtered.c_str());
                    break;
                }
                case 2: {
                    int ii = 2;
                    while (in_buff[ii] != '\n' && ii < (int)in_leng) name += in_buff[ii++];
                    ii++;
                    while (in_buff[ii] != '\n' && ii < (int)in_leng) message += in_buff[ii++];
                    name = frombase64(name);
                    message = frombase64(message);
                    cout << name << endl << message << endl;
                    filtered = filter(message);
                    if (filtered != message) users[userindex[name]].violated++;
                    chat_history += name + ':' + filtered + '\n';
                    if (users[userindex[name]].violated >= 3) {
                        for (auto it = udpcli.begin(); it != udpcli.end(); it++) {
                            if (it->first == ntohs(cliaddr.sin_port) && it->second == 2) {
                                udpcli.erase(it);
                                break;
                            }
                        }
                    }
                    base64name = tobase64(name);
                    base64filtered = tobase64(filtered);
                    struct packet_header *header = (struct packet_header*) out_ver1;
                    struct packet_data *data_name = (struct packet_data*) (out_ver1 + sizeof(struct packet_header));
                    struct packet_data *data_msg = (struct packet_data*) (out_ver1 + sizeof(struct packet_header) + sizeof(struct packet_data) + name.size());
                    header->flag = 0x01;
                    header->version = 0x01;
                    data_name->len = htons(name.size());
                    memcpy(data_name->data, name.c_str(), name.size());
                    data_msg->len = htons(filtered.size());
                    memcpy(data_msg->data, filtered.c_str(), filtered.size());
                    sprintf(out_ver2, "\x01\x02%s\n%s\n", base64name.c_str(), base64filtered.c_str());
                }
            }
            // if (skip) continue;
            for (auto i : udpcli) {
                cliaddr.sin_port = htons(i.first);
                clilen = sizeof(cliaddr);
                if (i.second == 1) sendto(udpfd, out_ver1, name.size() + filtered.size() + 6, 0, (sockaddr *) &cliaddr, clilen);
                else if (i.second == 2) sendto(udpfd, out_ver2, base64name.size() + base64filtered.size() + 4, 0, (sockaddr *) &cliaddr, clilen);
            }
        }
    }
    return 0;
}
