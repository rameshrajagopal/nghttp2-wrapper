#include <http2_client_wrap.h>

using namespace std;

int main(void)
{
    Http2Client client;

    client.connect("127.0.0.1", "8000");
    cout << "Exiting from Client" << endl;
    char data[16] = {0};
    memset(data, 'c', sizeof(data)-1);
    client.send("POST", "http://127.0.0.1:8000/", data);
    client.disconnect();
    getchar();
    return 0;
}
