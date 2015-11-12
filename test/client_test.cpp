#include <http2_client_wrap.h>
#include <thread>
#include <atomic>

using namespace std;

int main(int argc, char * argv[])
{
    std::atomic<int> num_failures {0};

    if (argc != 5) {
        cout << "Usage: " << endl;
        cout << argv[0] << " ip port " << endl;
        return -1;
    }
    cout << "Connecting to the server " << endl;
    string master_ip = argv[1];
    string master_port = argv[2];
    int num_requests = atoi(argv[3]);
    int num_users = atoi(argv[4]);
    for (int num = 0; num < num_users; ++num) {
        std::thread th([&num_failures, num_requests, master_ip, master_port]() {
            Http2Client client;
            client.connect(master_ip, master_port);
            for (int num = 0; num < num_requests; ++num) {
                char data[1024] = {0};
                memset(data, 'c', sizeof(data)-1);
                string uri = "http://" + master_ip + ":" + master_port + "/";
                if (client.send("POST", uri, data) != 0) {
                   ++num_failures;
                }
            }
//            client.disconnect();
        });
        th.detach();
    }
    getchar();
    cout << "num failures " << num_failures << endl;
    return 0;
}
