#include "headers.h"

#include <netinet/in.h>

void readRequest(string& method, string& url)
{
    cout << "\nInput method of request:" << endl;
    cin >> method;
    cout << "Input url (path to file):" << endl;
    cin >> url; 
}

class Client 
{
public:
    Client(string& m, string& u);
    ~Client();

    void createConnect(void);

private:
    string createRequest(string& method, string& url);

    int sock;
    string method, url;
};

Client::Client(string& m, string& u) : method(m), url(u) 
{
    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock < 0)
	{
		throw runtime_error("error in socket(): " + string(strerror(errno)));
	}
}

Client::~Client()
{
    close(sock);
}

void Client::createConnect(void)
{
    struct hostent* host = gethostbyname(HOST);
    if (!host)
    {
        throw runtime_error("error in gethostbyname()");
    }

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr = *((struct in_addr*) host->h_addr_list[0]);
    int server_size = sizeof(server);

    if (connect(sock, (struct sockaddr*) &server, sizeof(server)) < 0)
    {
        throw runtime_error("error in connect()");
    }

    char buffer[BUFSIZE] = { 0 };
    string request = createRequest(method, url), response = "";
    sendto(sock, request.c_str(), request.length(), 0, (struct sockaddr*) &server, server_size);
    while (recvfrom(sock, buffer, BUFSIZE, 0, (struct sockaddr*) &server, (socklen_t*) &server_size) > 0) 
    {
        response += string(buffer);
        memset(buffer, 0, BUFSIZE);
    }
    cout << response;
}

string Client::createRequest(string& method, string& url)
{
    string result = "";
    result += method + " ";
    result += url + " ";
    result += "HTTP/1.1\r\n";
    result += "Host: localhost:" + to_string(PORT) + "\r\n\r\n"; 
    return result;
}

int main(void)
{
    bool flag = true;
    char operation = 0;
    string method = "", url = "";
    while (flag)
    {
        cout << "1 - Input request" << endl;
        cout << "2 - Exit" << endl;
        cin >> operation;
        if (operation == '1')
        {
            readRequest(method, url);
            try
            {
                Client client(method, url);
                client.createConnect();
            }
            catch(const std::exception& e)
            {
                std::cerr << e.what() << '\n';
            }
        }
        else if (operation == '2')
            flag = false;
        else
            cout << "Input error" << endl;
    }
    return 0;
}
