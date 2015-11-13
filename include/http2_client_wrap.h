#ifndef __HTTP2_CLIENT_WRAP_H__
#define __HTTP2_CLIENT_WRAP_H__

#include <iostream>
#include <mutex>
#include <thread>
#include <nghttp2/asio_http2_client.h>
#include <condition_variable>
using namespace std;
using namespace nghttp2::asio_http2;
using namespace nghttp2::asio_http2::client;
using boost::asio::ip::tcp;

#define SYSLOG(fmt...) 

typedef 
function<void(const string & method, const string & uri, const string & data)> request_post_cb;
typedef 
function<void(void)> session_disconnect_cb;

enum class CallProgress {IN_PROGRESS, DONE};
enum class CallStatus {SUCCESS, ERROR};
class Http2Client {
public:
    Http2Client(): progress(CallProgress::IN_PROGRESS) {}

    int connect(const string & addr, const string & port) {
        this->status = CallStatus::SUCCESS;
        int sess_id = 0;
        std::thread th([&addr, &port, this, &sess_id](){
            /* create a session under this io service */
            std::unique_lock<std::mutex> mlock(sess_mutex);
            sessions.push_back(session(io_service, addr, port));
            sess_id = sessions.size()-1;
            mlock.unlock();
            sessions[sess_id].on_connect([this](tcp::resolver::iterator endpoint_it) {
               cout << "connected" << endl;
               notify_one(CallStatus::SUCCESS);
               SYSLOG(LOG_INFO, "connection establised"); 
            });
            sessions[sess_id].on_error([this](const boost::system::error_code & ec) {
                notify_one(CallStatus::ERROR);
                SYSLOG(LOG_INFO, "session connection error: %s\n", ec.value());
            });
            io_service.run();
            cout << "****service exiting " << endl;
        });
        th.detach();
        std::unique_lock<std::mutex> mlock(mutex_);
        while (progress == CallProgress::IN_PROGRESS) {
            cond_.wait(mlock);
        }
        mlock.unlock();
        return sess_id;
        /* throw an exception if the call was an error */
    }
    /* add data generator callback */
    int send(int sess_id, string method, string uri, string data)
    {
        std::unique_lock<std::mutex> mlock(mutex_);
        progress = CallProgress::IN_PROGRESS;
        mlock.unlock();
        io_service.post([method, uri, data, this, sess_id]() {
                boost::system::error_code ec;
                header_map h;
                auto req = sessions[sess_id].submit(ec, method, uri, h); 
                req->on_response([this](const response & res) {
                    if (res.status_code() != 200) {
                        cout << "rc: " << res.status_code() << endl;
                        notify_one(CallStatus::ERROR);
                    }
                    res.on_data([this](const uint8_t * data, size_t len) {
                        if (len == 0) {
                            notify_one(CallStatus::SUCCESS);
                        }
                        });
                    });
                req->on_close([this](uint32_t status) {
                    if (status != 0) {
                        cout << "req close error " << status << endl;
                        notify_one(CallStatus::ERROR);
                    }
                });
        });
        mlock.lock();
        while (progress == CallProgress::IN_PROGRESS) {
            cond_.wait(mlock);
        }
        mlock.unlock();
        return (int)status;
    }
    void disconnect(int sess_id) {
        cout << "disconnecting from service" << endl;
        io_service.post([this, sess_id]() {
            sessions[sess_id].shutdown();
        });
    }
    /* disconnect the call */
private:
    void notify_one(CallStatus status) {
        std::unique_lock<std::mutex> mlock(mutex_);
        this->progress = CallProgress::DONE;
        this->status = status;
        mlock.unlock();
        this->cond_.notify_one();
    }
    std::atomic<int> sessio_id;
    CallProgress progress;
    CallStatus status;
    std::mutex mutex_;
    std::condition_variable cond_;
    boost::asio::io_service io_service;
    request_post_cb req_post_cb {nullptr};
    vector<session> sessions;
    std::mutex sess_mutex;
    session_disconnect_cb sess_disconnect_cb {nullptr};
};


#endif /*__HTTP2_CLIENT_WRAP_H__*/
