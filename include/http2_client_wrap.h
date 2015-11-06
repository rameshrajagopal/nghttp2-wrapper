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

    void connect(const string & addr, const string & port) {
        cout << "trying to connect " << endl;
        this->status = CallStatus::SUCCESS;
        std::thread th([&addr, &port, this](){
            session sess(io_service, addr, port);
            sess.on_connect([&sess, this](tcp::resolver::iterator endpoint_it) {
               std::unique_lock<std::mutex> mlock(mutex_);
               this->progress = CallProgress::DONE;
               mlock.unlock();
               this->cond_.notify_one();
               cout << "connected" << endl;
               SYSLOG(LOG_INFO, "connection establised"); 
               req_post_cb = [&sess](const string & method, 
                                    const string & uri, const string & body) {
                  boost::system::error_code ec;
                  header_map h;
                  auto req = sess.submit(ec, method, uri, h); 
                     req->on_response([](const response & res) {
                     cout << "received response " << endl;
                  });
                  req->on_close([](uint32_t status) {
                     cout << "req got closed" << endl;
                  });
               };
               sess_disconnect_cb = [&sess]() {
                  cout << "shutting down the session" << endl;
                  sess.shutdown();                  
               };
            });
            sess.on_error([&sess, this](const boost::system::error_code & ec) {
                std::unique_lock<std::mutex> mlock(mutex_);
                this->progress = CallProgress::DONE;
                this->status = CallStatus::ERROR;
                mlock.unlock();
                this->cond_.notify_one();
                SYSLOG(LOG_INFO, "session connection error: %s\n", ec.value());
            });
            io_service.run();
        });
        th.detach();
        std::unique_lock<std::mutex> mlock(mutex_);
        while (progress == CallProgress::IN_PROGRESS) {
            cond_.wait(mlock);
        }
        mlock.unlock();
        /* throw an exception if the call was an error */
    }
    /* add data generator callback */
    void send(string method, string uri, string data) {
        if (this->req_post_cb) {
            io_service.post([method, uri, data, this]() {
                cout << method << " " << uri << " " << data << endl;
                req_post_cb(method, uri, data);
            });
        }
    }
    void disconnect() {
       if (this->sess_disconnect_cb) {
          io_service.post([this]() {
            sess_disconnect_cb();
          });
       }
    }
    /* disconnect the call */
private:
    std::atomic<int> sessio_id;
    CallProgress progress;
    CallStatus status;
    std::mutex mutex_;
    std::condition_variable cond_;
    boost::asio::io_service io_service;
    request_post_cb req_post_cb {nullptr};
    session_disconnect_cb sess_disconnect_cb {nullptr};
};


#endif /*__HTTP2_CLIENT_WRAP_H__*/
