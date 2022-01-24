#include <iostream>
#include <sstream>
#include <vector>
#include <map>
#include <queue>
#include <string>
#include <cstring>
#include <cstdio>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/unistd.h>
#include <netdb.h>
#include <errno.h>

#define	LISTENQ	1024
#define MAXLINE 4096
#define WRONG_USAGE 1
#define USER_EXISTANCE 2
#define WRONG_PASSWORD 3
#define WRONG_STATUS 4

using namespace std;

struct user {
    string username;
    string password;
    map<string, queue<string> > mailbox;
    int mails = 0;
    user() {}
    user(string a, string b) {
        username = a;
        password = b;
    }
};

int main(int argc, char **argv)
{
    if (argc != 2) {
        cout << "usage: hw1 [port]" << endl;
        return ENOEXEC;
    }
	int	listenfd, connfd, port_int;
	struct sockaddr_in servaddr;
	char in_buff[MAXLINE], out_buff[MAXLINE];
    stringstream ss;
    ss << argv[1];
    ss >> port_int;
	listenfd = socket(AF_INET, SOCK_STREAM, 0);

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family      = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port        = htons(port_int);	/* daytime server */

	bind(listenfd, (sockaddr *) &servaddr, sizeof(servaddr));

	listen(listenfd, LISTENQ);

    char welcomemessage[MAXLINE] = "********************************\n** Welcome to the BBS server. **\n********************************\n% ";
    char prompt[3] = "% ";
    vector<struct user> users;
    users.emplace_back(user());
    map<string, int> userindex;
	while (1) {
		connfd = accept(listenfd, (sockaddr *) NULL, NULL);
        bool logged = false;
        string logged_user;
        write(connfd, welcomemessage, strlen(welcomemessage));
        while (read(connfd, in_buff, sizeof(in_buff))) {
            stringstream ss;
            string func;
            bzero(out_buff, MAXLINE);
            ss << in_buff;
            ss >> func;
            if (func == "register") {
                int error = 0;
                string username, password, laststring;
                if (ss >> username) {
                    if (userindex[username]) error = USER_EXISTANCE;
                    else if (ss >> password)
                    else error = WRONG_USAGE;
                }
                else error = WRONG_USAGE;
                switch (error) {
                    case WRONG_USAGE:
                        snprintf(out_buff, sizeof(out_buff), "Usage: register <username> <password>\n");
                        break;
                    case USER_EXISTANCE:
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
                string username, password, laststring;
                if (logged) error = WRONG_STATUS;
                else if (ss >> username) {
                    if (!userindex[username]) error = USER_EXISTANCE;
                    else if (ss >> password) {
                        if (password != users[userindex[username]].password) error = WRONG_PASSWORD;
                    }
                    else error = WRONG_USAGE;
                }
                else error = WRONG_USAGE;
                switch(error) {
                    case WRONG_USAGE:
                        snprintf(out_buff, sizeof(out_buff), "Usage: login <username> <password>\n");
                        break;
                    case USER_EXISTANCE:
                        snprintf(out_buff, sizeof(out_buff), "Login failed.\n");
                        break;
                    case WRONG_PASSWORD:
                        snprintf(out_buff, sizeof(out_buff), "Login failed.\n");
                        break;
                    case WRONG_STATUS:
                        snprintf(out_buff, sizeof(out_buff), "Please logout first.\n");
                        break;
                    default:
                        logged = true;
                        logged_user = username;
                        snprintf(out_buff, sizeof(out_buff), "Welcome, %s.\n", logged_user.c_str());
                }
            }
            else if (func == "logout") {
                if (!logged) snprintf(out_buff, sizeof(out_buff), "Please login first.\n");
                else {
                    logged = false;
                    snprintf(out_buff, sizeof(out_buff), "Bye, %s.\n", logged_user.c_str());
                }
            }
            else if (func == "whoami") {
                if (logged) snprintf(out_buff, sizeof(out_buff), "%s\n", logged_user.c_str());
                else snprintf(out_buff, sizeof(out_buff), "Please login first.\n");
            }
            else if (func == "list-user") {
                for (auto it = userindex.begin(); it != userindex.end(); it++) {
                    if (it->second) {
                        snprintf(out_buff, sizeof(out_buff), "%s\n", it->first.c_str());
                        write(connfd, out_buff, strlen(out_buff));
                    }
                }
            }
            else if (func == "send") {
                int error = 0;
                string username, message;
                if (ss >> username) {
                    if (!logged) error = WRONG_STATUS;
                    else if (!userindex[username]) error = USER_EXISTANCE;
                    else {
                        getline(ss, message);
                        if (message.length() <= 2) error = WRONG_USAGE;
                        else {
                            message = message.substr(2, message.size() - 3);
                            users[userindex[username]].mailbox[logged_user].push(message);
                            users[userindex[username]].mails++;
                        }
                    }
                }
                else error = WRONG_USAGE;
                switch(error) {
                    case WRONG_USAGE:
                        snprintf(out_buff, sizeof(out_buff), "Usage: send <username> <message>\n");
                        break;
                    case USER_EXISTANCE:
                        snprintf(out_buff, sizeof(out_buff), "User not existed.\n");
                        break;
                    case WRONG_STATUS:
                        snprintf(out_buff, sizeof(out_buff), "Please login first.\n");
                        break;
                }
            }
            else if (func == "list-msg") {
                if (!logged);
                else if (users[userindex[logged_user]].mails == 0) {
                    snprintf(out_buff, sizeof(out_buff), "Your message box is empty.\n");
                    write(connfd, out_buff, strlen(out_buff));
                }
                else {
                    int i = userindex[logged_user];
                    for (auto it = users[i].mailbox.begin(); it != users[i].mailbox.end(); it++) {
                        if (it->second.size()) {
                            snprintf(out_buff, sizeof(out_buff), "%d message from %s\n", it->second.size(), it->first.c_str());
                            write(connfd, out_buff, strlen(out_buff));
                        }
                    }
                }
            }
            else if (func == "receive") {
                if (!logged) logged_user = "";
                string username;
                ss >> username;
                if (!userindex[username]) snprintf(out_buff, sizeof(out_buff), "User not existed.\n");
                else {
                    snprintf(out_buff, sizeof(out_buff), "%s\n", users[userindex[logged_user]].mailbox[username].front().c_str());
                    if (users[userindex[logged_user]].mailbox[username].size()) {
                        users[userindex[logged_user]].mailbox[username].pop();
                        users[userindex[logged_user]].mails--;
                    }
                }
            }
            else if (func == "exit") {
                if (logged) {
                    snprintf(out_buff, sizeof(out_buff), "Bye, %s.\n", logged_user.c_str());
                    write(connfd, out_buff, strlen(out_buff));
                }
                break;
            }
            bzero(in_buff, MAXLINE);
            if (func != "list-user" && func != "list-msg") write(connfd, out_buff, strlen(out_buff));
            write(connfd, prompt, strlen(prompt));
        }
		close(connfd);
	}
    return 0;
}