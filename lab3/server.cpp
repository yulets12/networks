#include "headers.h"

#include <ctime>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <map>

#define STATLOG "logs/statistics.txt"

struct Request
{
    string method, url, version;
    map<string, string> headers;
    Request() : method(""), url(""), version("") {}
    Request(string& m, string& u, string& v, map<string, string>& h) :
        method(m), url(u), version(v), headers(h) {}
};

class Server
{
public:
    Server();
    ~Server();

    void startServer(void);

private:
    void handleClient(int client);

    thread& getWorker(void);
    void freeWorker(thread& worker);
    bool poolEmpty(int* index);

    Request* parseRequest(char* buffer, int size);
    string createResponse(Request* request);
    string writeResponse(string& code, string& type, int length, const string& body);

    void statistics(void);

    int sock, pool_size;
    vector<pair<thread, bool>> threadPool;
    mutex mtx;
    condition_variable cond;
};

Server::Server()
{
    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock < 0)
	{
		throw runtime_error("error in socket(): " + string(strerror(errno)));
	}
    pool_size = THREADS;
    for (int i = 0; i < pool_size; i++)
    {
        threadPool.push_back(make_pair(thread(), true));
    }
}

Server::~Server()
{
    for (int i = 0; i < pool_size; i++)
    {
        if (threadPool[i].first.joinable())
        {
            threadPool[i].first.join();
        }
    }
    close(sock);
}

void Server::startServer(void)
{
    struct sockaddr_in server;
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(PORT);
	if (bind(sock, (struct sockaddr *) &server, sizeof(server)) < 0)
	{
		close(sock);
		throw runtime_error("error in bind(): " + string(strerror(errno)));
	}
    listen(sock, pool_size);

    struct sockaddr client;
	int client_size = sizeof(client), client_socket = 0;
    while ((client_socket = accept(sock, &client, (socklen_t*) &client_size)))
    {
        statistics();
        thread& worker = getWorker();
        worker = thread([this, client_socket] { handleClient(client_socket); });
        if (worker.joinable())
            freeWorker(worker);
    }
}

bool Server::poolEmpty(int* index)
{
    if (!index)
        throw runtime_error("bad pointer");

    bool result = true;
    int i = 0;
    while (result && i < pool_size)
    {
        if (threadPool[i].second == true)
        {
            result = false;
            *index = i;
        }
        i++;
    }
    return result;
}

thread& Server::getWorker(void)
{
    int i = -1;
    unique_lock<mutex> lock(mtx);
    while (poolEmpty(&i))
        cond.wait(lock);

    threadPool[i].second = false;
    return threadPool[i].first;
}

void Server::freeWorker(thread& worker)
{
    worker.join();
    unique_lock<mutex> lock(mtx);
    for (int i = 0; i < pool_size; i++)
        if (worker.get_id() == threadPool[i].first.get_id())
            threadPool[i].second = true;
    lock.unlock();
    cond.notify_one();
}

Request* Server::parseRequest(char* buffer, int size)
{
    char* save = NULL;
    string m(strtok_r(buffer, " ", &save));
    string u(strtok_r(NULL, " ", &save));
    string v(strtok_r(NULL, "\r\n", &save));
    map<string, string> h;
    char* header = NULL;
    while ((header = strtok_r(NULL, "\r\n", &save)))
    {
        char* end_header = strchr(header, ':');
        char* end_value = strstr(header, "\r\n");
        size_t len1 = end_header - header, len2 = strlen(header) - len1 - 2;
        //cout << header << " " << end_header << endl;
        h[string(header, len1)] = string(header + len1 + 2, len2);
        cout << "here" << endl;
    }
    cout << m << endl;
    cout << u << endl;
    cout << v << endl;
    return new Request(m, u, v, h);
}

string Server::createResponse(Request* request)
{
    map<string, string> types = {
        { "html", "text/html" },
        { "txt", "text/plain"},
        { "css", "text/css"},
        { "jpg", "image/jpeg"},
        { "jpeg", "image/jpeg"},
        { "png", "image/png"},
        { "js", "application/javascript"},
    };

    string code = request->version;
    if (request->method == "" || request->version == "" || request->url == "")
    {
        code += " 502 Bad GateWay";
        return writeResponse(code, types["html"], 0, "");
    }

    if (request->method != "GET")
    {
        code += " 405 Method Not Allowed";
        return writeResponse(code, types["html"], 0, "");
    }

    if ((request->url).find("../") != string::npos)
    {
        code += " 403 Forbidden";
        return writeResponse(code, types["html"], 0, "");
    }
    string file_type = "";
    int dot_pos = -1;
    if ((dot_pos = (request->url).rfind(".")) == string::npos)
    {
        code += " 400 Bad Request";
        return writeResponse(code, types["html"], 0, "");
    }
    else
        file_type = (request->url).substr(dot_pos, (request->url).length() - dot_pos);
    

    FILE *f = fopen((request->url).c_str(), "rb");
    if (!f)
    {
        code += " 404 Not Found";
        return writeResponse(code, types["html"], 0, "");
    }
    string file = "";
    char byte = 0;
    while (fread(&byte, sizeof(byte), 1, f))
        file += byte;

    code += " 200 OK";
    return writeResponse(code, types[file_type], file.length(), file);
}

string Server::writeResponse(string& code, string& type, int length, const string& body)
{
    string result = "";
    result += code + "\r\n";
    result += "Server: " + string(HOST) + "\r\n";
    result += "Connection: close\r\n";
    result += "Content-Type: " + type + "\r\n";
    result += "Content-Length: " +  to_string(length) + "\r\n\r\n";
    result += body;
    return result;
}

void Server::handleClient(int client)
{
    char buffer[BUFSIZE];
    int request_size = recv(client, buffer, BUFSIZE, 0);
    if (request_size <= 0)
    {
        close(client);
        throw runtime_error("Error in request");
    }
    cout << "\nRequest:\n" << buffer;
    Request *request = parseRequest(buffer, request_size);
    string response = createResponse(request);
    cout << "\nResponse:\n" << response;
    send(client, response.c_str(), response.length(), 0);
    delete request;
    close(client);
}

void Server::statistics(void)
{
    FILE *f = fopen(STATLOG, "r");
    if (!f)
        return;

    string file = "";
    char byte = 0;
    while (fread(&byte, sizeof(byte), 1, f))
        file += byte;  
    fclose(f);  

    time_t t = time(NULL);
    struct tm* local = localtime(&t);
    int weekday = local->tm_wday;
    vector<string> days = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    size_t day_pos = file.find(days[weekday]);
    if (day_pos == string::npos)
        file += days[weekday] + ": 1\n";
    else
    {
        size_t end_number = file.find("\n", day_pos);
        day_pos += days[weekday].length() + 2;
        int count = stoi(file.substr(day_pos, end_number - day_pos)) + 1;
        file.replace(day_pos, end_number - day_pos, to_string(count));
    }
    f = fopen(STATLOG, "wb");
    fputs(file.c_str(), f);
    fclose(f);
}

int main(void)
{
    try 
    {
        Server server;
        server.startServer();
    }
    catch (exception& e)
    {
        cout << "Exception: " << e.what() << endl;
    }
    return 0;
}
