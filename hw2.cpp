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

#define	LISTENQ	1024
#define MAXLINE 4096
#define MAXCLI 10

#define WRONG_USAGE 1
#define WRONG_PASSWORD 2
#define WRONG_STATUS 3
#define EXISTANCE 4
#define NOT_AUTHOR 5

using namespace std;

typedef struct post{
    bool exist = false;
    int sn;
    string author;
    string title;
    string date;
    string context;
    string comment;
    post() {}
    post(int i, string a, string t, string d, string c) {
        exist = true;
        sn = i;
        author = a;
        title = t;
        date = d;
        context = c;
        size_t pos = context.find("<br>");
        while (pos != string::npos) {
            context.replace(context.find("<br>"), 4, "\n");
            pos = context.find("<br>", pos + 1);
        }
    }
} post;

typedef struct board{
    string name;
    string moderator;
    vector<post*> posts;
    board() {}
    board(string s, string u) {
        name = s;
        moderator = u;
    }
} board;

typedef struct user{
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
        cout << "usage: hw1 [port]" << endl;
        return ENOEXEC;
    }

	/* initialize sockets */
    int i, cli_cnt, maxfd, listenfd, connfd, sockfd, port_int;
    int	nready, client[MAXCLI];
    fd_set rset, allset;
    char in_buff[MAXLINE], out_buff[MAXLINE];
    socklen_t clilen;
    struct sockaddr_in cliaddr, servaddr;
    stringstream ss;
    ss << argv[1];
    ss >> port_int;
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    int enable = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port        = htons(port_int);

    bind(listenfd, (sockaddr *) &servaddr, sizeof(servaddr));

    listen(listenfd, LISTENQ);

    maxfd = listenfd;			/* initialize */
    cli_cnt = -1;					/* index into client[] array */
    for (i = 0; i < MAXCLI; i++) {
        client[i] = -1;			/* -1 indicates available entry */
    }
    FD_ZERO(&allset);
    FD_SET(listenfd, &allset);

    /* welcome and prompt */ 
    char welcomemessage[MAXLINE] = "********************************\n** Welcome to the BBS server. **\n********************************\n% ";
    char prompt[3] = "% ";

    /* initialize app */
    vector<bool> logged(MAXCLI);
    vector<string> logged_user(MAXCLI);
    map<string, int> userindex;
    map<string, int> boardindex;
    map<string, bool> online;
    vector<user> users;
    vector<board> boards;
    vector<post> posts;
    users.emplace_back(user());
    boards.emplace_back(board());
    posts.emplace_back(post());

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
                                string username, password;
                                if (ss >> username) {
                                    if (ss >> password) {  
                                        if (logged[i] || online[username]) error = WRONG_STATUS;
                                        else if (!userindex[username]) error = EXISTANCE;
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
                                if (!logged[i]) snprintf(out_buff, sizeof(out_buff), "Please login first.\n");
                                else {
                                    snprintf(out_buff, sizeof(out_buff), "Bye, %s.\n", logged_user[i].c_str());
                                    logged[i] = false;
                                    online[logged_user[i]] = false;
                                    logged_user[i] = "";
                                }
                            }
                            else if (func == "exit") {
                                if (logged[i]) {
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
                            else if (func == "create-board") {
                                int error = 0;
                                string name;
                                if (!(ss >> name)) error = WRONG_USAGE;
                                else if (!logged[i]) error = WRONG_STATUS;
                                else if (boardindex[name]) error = EXISTANCE;
                                switch (error) {
                                    case WRONG_USAGE:
                                        snprintf(out_buff, sizeof(out_buff), "Usage: create-board <name>\n");
                                        break;
                                    case WRONG_STATUS:
                                        snprintf(out_buff, sizeof(out_buff), "Please login first.\n");
                                        break;
                                    case EXISTANCE:
                                        snprintf(out_buff, sizeof(out_buff), "Board already exists.\n");
                                        break;
                                    default:
                                        boardindex[name] = boards.size();
                                        boards.emplace_back(board(name, logged_user[i]));
                                        snprintf(out_buff, sizeof(out_buff), "Create board successfully.\n");
                                }
                            }
                            else if (func == "create-post") {
                                int error = 0;
                                string in, board, title, content, date;
                                bool titling = false, contenting = false, nottitled = true, notcontented = true;
                                if (ss >> board) {
                                    while (ss >> in) {
                                        if (in == "--title") {
                                            titling = true;
                                            contenting = false;
                                            nottitled = false;
                                        }
                                        else if (in == "--content") {
                                            contenting = true;
                                            titling = false;
                                            notcontented = false;
                                        }
                                        else if (titling || contenting) {
                                            if (titling) title += in + " ";
                                            else if (contenting) content += in + " ";
                                        }
                                        else error = WRONG_USAGE;
                                    }
                                    if (nottitled || notcontented) error = WRONG_USAGE;
                                    if (error != WRONG_USAGE && !logged[i]) error = WRONG_STATUS;
                                    if (error != WRONG_USAGE && error != WRONG_STATUS && !boardindex[board]) error = EXISTANCE;
                                }
                                else error = WRONG_USAGE;
                                switch (error) {
                                    case WRONG_USAGE:
                                        snprintf(out_buff, sizeof(out_buff), "Usage: create-post <board-name> --title <title> --content <content>\n");
                                        break;
                                    case WRONG_STATUS:
                                        snprintf(out_buff, sizeof(out_buff), "Please login first.\n");
                                        break;
                                    case EXISTANCE:
                                        snprintf(out_buff, sizeof(out_buff), "Board does not exist.\n");
                                        break;
                                    default:
                                        time_t rawtime;
                                        struct tm* timeinfo;
                                        char buf[6];
                                        time(&rawtime);
                                        timeinfo = localtime (&rawtime);
                                        strftime(buf, 6, "%m/%d.",timeinfo);
                                        date = buf;
                                        posts.emplace_back(post(posts.size(), logged_user[i], title, date, content));
                                        cout << posts.back().sn << posts.back().author << posts.back().title << posts.back().date << '\"' << posts.back().context << '\"' << endl;
                                        boards[boardindex[board]].posts.emplace_back(&posts.back());
                                        snprintf(out_buff, sizeof(out_buff), "Create post successfully.\n");
                                }

                            }
                            else if (func == "list-board") {
                                string out = "Index Name Moderator\n";
                                for (auto i:boards) {
                                    if (boardindex[i.name]) {
                                        stringstream iss;
                                        iss << boardindex[i.name];
                                        string tmp;
                                        iss >> tmp;
                                        out += tmp + " " + i.name + " " + i.moderator + "\n";
                                    }
                                    
                                }
                                snprintf(out_buff, sizeof(out_buff), out.c_str());
                            }
                            else if (func == "list-post") {
                                int error = 0;
                                string board, out = "S/N Title Author Date\n";
                                if (ss >> board) {
                                    if (!boardindex[board]) error = EXISTANCE;
                                }
                                else error = WRONG_USAGE; 
                                switch (error) {
                                    case WRONG_USAGE:
                                        snprintf(out_buff, sizeof(out_buff), "Usage: list-post <board-name>\n");
                                        break;
                                    case EXISTANCE:
                                        snprintf(out_buff, sizeof(out_buff), "Board does not exist.\n");
                                        break;
                                    default:
                                        for (auto j:boards[boardindex[board]].posts) {
                                            if (j->exist) {
                                                stringstream iss;
                                                int sn = j->sn;
                                                iss << sn;
                                                string tmp;
                                                iss >> tmp;
                                                out += tmp + ' ' + posts[sn].title + posts[sn].author + ' ' + posts[sn].date + "\n";
                                                cout << out << endl;
                                            }
                                        }
                                        snprintf(out_buff, sizeof(out_buff), out.c_str());
                                }
                            }
                            else if (func == "read") {
                                int tgtsn, error = 0;
                                if (ss >> tgtsn) {
                                    if (posts.size() <= tgtsn || !posts[tgtsn].exist) error = EXISTANCE;
                                }
                                else error = WRONG_USAGE;
                                switch (error) {
                                    case WRONG_USAGE:
                                        snprintf(out_buff, sizeof(out_buff), "Usage: read <post-S/N>\n");
                                        break;
                                    case EXISTANCE:
                                        snprintf(out_buff, sizeof(out_buff), "Post does not exist.\n");
                                        break;
                                    default:
                                        string out;
                                        out += "Author: " + posts[tgtsn].author
                                            + "\nTitle: " + posts[tgtsn].title
                                            + "\nDate: " + posts[tgtsn].date
                                            + "\n--\n"
                                            + posts[tgtsn].context
                                            + "\n--\n";
                                        if (posts[tgtsn].comment != "") {
                                            out += posts[tgtsn].comment; 
                                        }
                                        snprintf(out_buff, sizeof(out_buff), out.c_str());
                                }
                            }
                            else if (func == "delete-post") {
                                int error = 0, tgtsn;
                                if (ss >> tgtsn) {
                                    if (!logged[i]) error = WRONG_STATUS;
                                    else if (posts.size() <= tgtsn || !posts[tgtsn].exist) error = EXISTANCE;
                                    else if (posts[tgtsn].author != logged_user[i]) error = NOT_AUTHOR;
                                }
                                else error = WRONG_USAGE;
                                switch (error) {
                                    case WRONG_USAGE:
                                        snprintf(out_buff, sizeof(out_buff), "Usage: delete-post <post-S/N>\n");
                                        break;
                                    case WRONG_STATUS:
                                        snprintf(out_buff, sizeof(out_buff), "Please login first.\n");
                                        break;
                                    case EXISTANCE:
                                        snprintf(out_buff, sizeof(out_buff), "Post does not exist.\n");
                                        break;
                                    case NOT_AUTHOR:
                                        snprintf(out_buff, sizeof(out_buff), "Not the post owner.\n");
                                        break;
                                    default:
                                        posts[tgtsn].exist = false;
                                        snprintf(out_buff, sizeof(out_buff), "Delete successfully.\n");
                                }
                            }
                            else if (func == "update-post") {
                                int error = 0, tgtsn;
                                string in, title, content;
                                bool titling = false, contenting = false;
                                if (ss >> tgtsn) {
                                    while (ss >> in) {
                                        if (titling) {
                                            title += in + " ";
                                        }
                                        else if (contenting) {
                                            content += in + " ";
                                        }
                                        else if (in == "--title") titling = true;
                                        else if (in == "--content") contenting = true;
                                    }
                                    if ((!titling && !contenting)) error = WRONG_USAGE;
                                    if (titling && title == "" || contenting && content == "") error = WRONG_USAGE;
                                    else if (!logged[i]) error = WRONG_STATUS;
                                    else if (posts.size() <= tgtsn || !posts[tgtsn].exist) error = EXISTANCE;
                                    else if (posts[tgtsn].author != logged_user[i]) error = NOT_AUTHOR;
                                }
                                else error = WRONG_USAGE;
                                switch (error) {
                                    case WRONG_USAGE:
                                        snprintf(out_buff, sizeof(out_buff), "Usage: update-post <post-S/N> --title/content <new>\n");
                                        break;
                                    case WRONG_STATUS:
                                        snprintf(out_buff, sizeof(out_buff), "Please login first.\n");
                                        break;
                                    case EXISTANCE:
                                        snprintf(out_buff, sizeof(out_buff), "Post does not exist.\n");
                                        break;
                                    case NOT_AUTHOR:
                                        snprintf(out_buff, sizeof(out_buff), "Not the post owner.\n");
                                        break;
                                    default:
                                        if (title != "") posts[tgtsn].title = title;
                                        if (content != "") posts[tgtsn].context = content;
                                        size_t pos = posts[tgtsn].context.find("<br>");
                                        while (pos != string::npos) {
                                            posts[tgtsn].context.replace(posts[tgtsn].context.find("<br>"), 4, "\n");
                                            pos = posts[tgtsn].context.find("<br>", pos + 1);
                                        }
                                        snprintf(out_buff, sizeof(out_buff), "Update successfully.\n");
                                }
                            }
                            else if (func == "comment") {
                                int error = 0, tgtsn;
                                string comment, in;
                                if (ss >> tgtsn) {
                                    while (ss >> in) {
                                        comment += in + " ";
                                    }
                                    if (comment == "") error = WRONG_USAGE;
                                    else if (!logged[i]) error = WRONG_STATUS;
                                    else if (posts.size() <= tgtsn || !posts[tgtsn].exist) error = EXISTANCE;
                                }
                                else error = WRONG_USAGE;
                                switch (error) {
                                    case WRONG_USAGE:
                                        snprintf(out_buff, sizeof(out_buff), "Usage: comment <post-S/N> <comment>\n");
                                        break;
                                    case WRONG_STATUS:
                                        snprintf(out_buff, sizeof(out_buff), "Please login first.\n");
                                        break;
                                    case EXISTANCE:
                                        snprintf(out_buff, sizeof(out_buff), "Post does not exist.\n");
                                        break;
                                    default:
                                        posts[tgtsn].comment += logged_user[i] + ": " + comment + "\n";
                                        snprintf(out_buff, sizeof(out_buff), "Comment successfully.\n");
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
    }
    return 0;
}