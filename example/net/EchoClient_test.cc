
/******************************************************
*   Copyright (C)2019 All rights reserved.
*
*   Author        : owb
*   Email         : 2478644416@qq.com
*   File Name     : EchoClient_test.cc
*   Last Modified : 2019-07-12 14:37
*   Describe      :
*
*******************************************************/

#include "ideal/net/TcpClient.h"
#include "ideal/net/TcpConnection.h"
#include "ideal/base/Logger.h"
#include "ideal/base/Thread.h"
#include "ideal/net/EventLoop.h"
#include "ideal/net/InetAddress.h"

#include <utility>
#include <boost/implicit_cast.hpp>

#include <stdio.h>
#include <unistd.h>


using namespace ideal;
using namespace ideal::net;

class EchoClient;
std::vector<std::unique_ptr<EchoClient>> clients;

int current = 0;

class EchoClient
{
public:
	EchoClient(EventLoop* loop, const InetAddress& listenAddr, const std::string& id) :
		loop_(loop),
		client_(loop, listenAddr, "EchoClient"+id) {
    	client_.setConnectionCallback(std::bind(&EchoClient::onConnection, this, _1));
    	client_.setMessageCallback(std::bind(&EchoClient::onMessage, this, _1, _2, _3));
	}

  	void connect() {
    	client_.connect();
  	}

private:
	void onConnection(const TcpConnectionPtr& conn) {
    	LOG_TRACE << conn->localAddress().toIpPort() << " -> "
        		  << conn->peerAddress().toIpPort() << " is "
                  << (conn->connected() ? "UP" : "DOWN");

    	if(conn->connected()) {
      		++current;
      		if(boost::implicit_cast<size_t>(current) < clients.size()) {
        		clients[current]->connect();
      		}
      		LOG_INFO << "*** connected " << current;
    	}
    	conn->send("world\n");
  	}

  	void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp time) {
    	std::string msg(buf->retrieveAllAsString());
    	LOG_TRACE << conn->name() << " recv " << msg.size() << " bytes at " << time.toString();
     	if(msg == "quit\n") {
      		conn->send("bye\n");
      		conn->shutdown();
    	}
    	else if(msg == "shutdown\n") {
      		loop_->quit();
    	}
    	else {
      		conn->send(msg);
    	}
  	}

private:
  	EventLoop* loop_;
  	TcpClient client_;
};

int main(int argc, char* argv[])
{
	LOG_INFO << "pid = " << getpid() << ", tid = " << CurrentThread::tid();
	if(argc > 1) {
		EventLoop loop;
		InetAddress serverAddr(argv[1], 2000);

		int n = 1;
		if(argc > 2) {
	  		n = atoi(argv[2]);
		}

		clients.reserve(n);
		for(int i = 0; i < n; ++i) {
	  		char buf[32];
	  		snprintf(buf, sizeof buf, "%d", i+1);
	  		clients.emplace_back(new EchoClient(&loop, serverAddr, buf));
		}

		clients[current]->connect();
		loop.loop();
	}
	else {
		printf("Usage: %s host_ip [current#]\n", argv[0]);
	}
}

