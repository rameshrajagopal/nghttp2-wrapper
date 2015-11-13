#include <http2_client_wrap.h>
#include <thread>
#include <atomic>

using namespace std;

typedef struct client_info_s
{
    int num;
    int failures;
    int min_time;
    int max_time;
    int64_t total;
}client_info_t;

int getdiff_time(struct timeval tstart, struct timeval tend)
{
    struct timeval tdiff;

    if (tend.tv_usec < tstart.tv_usec) {
        tdiff.tv_sec = tend.tv_sec - tstart.tv_sec - 1;
        tdiff.tv_usec = (1000000 - tstart.tv_usec) + tend.tv_usec;
    } else {
        tdiff.tv_sec = tend.tv_sec - tstart.tv_sec;
        tdiff.tv_usec = tend.tv_usec - tstart.tv_usec;
    }
    return (tdiff.tv_sec * 1000 + tdiff.tv_usec/1000);
}

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
    client_info_t info[num_users];

    for (int num = 0; num < num_users; ++num) {
        std::thread th([num_requests, master_ip, master_port, &info, num]() {
            Http2Client client;
            struct timeval tstart;
            struct timeval tend;
            info[num].min_time = INT_MAX;
            info[num].max_time = INT_MIN;
            info[num].total = 0;
            info[num].failures = 0;
            info[num].num = 0;

            int sess_id = client.connect(master_ip, master_port);
            for (int num = 0; num < num_requests; ++num) {
                char data[1024] = {0};
                memset(data, 'c', sizeof(data)-1);
                string uri = "http://" + master_ip + ":" + master_port + "/";
                gettimeofday(&tstart, NULL);
                if (client.send(sess_id, "POST", uri, data) != 0) {
                    ++info[num].failures;
                } else {
                    gettimeofday(&tend, NULL);
                    int diff_time = getdiff_time(tstart, tend);
                    if (diff_time < info[num].min_time) info[num].min_time = diff_time;
                    if (diff_time > info[num].max_time) info[num].max_time = diff_time;
                    info[num].total += diff_time;
                    ++info[num].num;
                }
            }
            client.disconnect(sess_id);
            getchar();
        });
        th.detach();
    }
    getchar();
    int failures = 0, min_time = INT_MAX, max_time = INT_MIN;
    int64_t total = 0;
    int total_requests = 0;

    for (int num = 0; num < num_users; ++num) {
        total += info[num].total;
        total_requests += info[num].num;
        if (info[num].min_time < min_time) min_time = info[num].min_time;
        if (info[num].max_time > max_time) max_time = info[num].max_time;
        failures += info[num].failures;
    }
    cout << "Total requests " << total_requests << endl;
    cout << "min val: " << min_time << endl;
    cout << "max val: " << max_time << endl;
    cout << "total time: " << total << endl;
    cout << "total errors: " << failures << endl;
    return 0;
}

