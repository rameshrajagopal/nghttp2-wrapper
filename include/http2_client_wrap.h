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
        this->status = CallStatus::SUCCESS;
        std::thread th([&addr, &port, this](){
            session sess(io_service, addr, port);
            sess.on_connect([&sess, this](tcp::resolver::iterator endpoint_it) {
               cout << "connected" << endl;
               notify_one(CallStatus::SUCCESS);

               SYSLOG(LOG_INFO, "connection establised"); 
               req_post_cb = [&sess, this](const string & method, 
                                    const string & uri, const string & body) {
                  boost::system::error_code ec;
                  header_map h;
                  auto req = sess.submit(ec, method, uri, h); 
                  req->on_response([this](const response & res) {
                     cout << "rc: " << res.status_code() << endl;
                     if (res.status_code() != 200) {
                        notify_one(CallStatus::ERROR);
                     }
                     res.on_data([this](const uint8_t * data, size_t len) {
                        if (len == 0) {
                            cout << "response data" << endl;
                            notify_one(CallStatus::SUCCESS);
                        }
                     });
                  });
                  req->on_close([this](uint32_t status) {
                     cout << "req got closed " << status << endl;
                     notify_one(CallStatus::ERROR);
                  });
               };
               sess_disconnect_cb = [&sess]() {
                  cout << "shutting down the session" << endl;
                  //sess.shutdown();                  
               };
            });
            sess.on_error([&sess, this](const boost::system::error_code & ec) {
                std::unique_lock<std::mutex> mlock(mutex_);
                notify_one(CallStatus::ERROR);
                SYSLOG(LOG_INFO, "session connection error: %s\n", ec.value());
            });
            io_service.run();
            cout << "****service exiting " << endl;
            while (1) {
                sleep(1);
            }
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
    int send(string method, string uri, string data) {
        if (this->req_post_cb) {
            std::unique_lock<std::mutex> mlock(mutex_);
            progress = CallProgress::IN_PROGRESS;
            mlock.unlock();
            io_service.post([method, uri, data, this]() {
                    cout << method << " " << uri << endl;
                    req_post_cb(method, uri, data);
                    });
            mlock.lock();
            while (progress == CallProgress::IN_PROGRESS) {
                cond_.wait(mlock);
            }
            mlock.unlock();
            return (int)status;
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
    session_disconnect_cb sess_disconnect_cb {nullptr};
};


#endif /*__HTTP2_CLIENT_WRAP_H__*/
