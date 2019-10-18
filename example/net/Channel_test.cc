
/******************************************************
*   Copyright (C)2019 All rights reserved.
*
*   Author        : owb
*   Email         : 2478644416@qq.com
*   File Name     : Channel_test.cc
*   Last Modified : 2019-07-06 11:08
*   Describe      :
*
*******************************************************/

#include "ideal/base/Logger.h"
#include "ideal/net/Channel.h"
#include "ideal/net/EventLoop.h"

#include <functional>
#include <map>

#include <stdio.h>
#include <unistd.h>
#include <sys/timerfd.h>

using namespace ideal;
using namespace ideal::net;

void print(const char* msg)
{
  static std::map<const char*, Timestamp> lasts;
  Timestamp& last = lasts[msg];
  Timestamp now = Timestamp::now();
  printf("%s tid %d %s delay %f\n", now.toString().c_str(), CurrentThread::tid(),
         msg, timeDifference(now, last));
  last = now;
}

namespace ideal
{
namespace net
{
int createTimerfd();
void readTimerfd(int timerfd, Timestamp now);
}
}

// Use relative time, immunized to wall clock changes.
class PeriodicTimer {
public:
  PeriodicTimer(EventLoop* loop, double interval, const TimerCallback& cb)
    : loop_(loop),
      timerfd_(ideal::net::createTimerfd()),
      timerfdChannel_(loop, timerfd_),
      interval_(interval),
      cb_(cb)
  {
    timerfdChannel_.setReadCallback(
        std::bind(&PeriodicTimer::handleRead, this));
    timerfdChannel_.enableReading();
  }

  void start()
  {
    struct itimerspec spec;
    memset(&spec, 0, sizeof spec);
    spec.it_interval = toTimeSpec(interval_);
    spec.it_value = spec.it_interval;
    int ret = ::timerfd_settime(timerfd_, 0 /* relative timer */, &spec, NULL);
    if (ret)
    {
      LOG_SYSERR << "timerfd_settime()";
    }
  }

  ~PeriodicTimer()
  {
    timerfdChannel_.disableAll();
    timerfdChannel_.remove();
    ::close(timerfd_);
  }

private:
  void handleRead()
  {
    loop_->assertInLoopThread();
    ideal::net::readTimerfd(timerfd_, Timestamp::now());
    if (cb_)
      cb_();
  }

  static struct timespec toTimeSpec(double seconds)
  {
    struct timespec ts;
    memset(&ts, 0, sizeof ts);
    const int64_t kNanoSecondsPerSecond = 1000000000;
    const int kMinInterval = 100000;
    int64_t nanoseconds = static_cast<int64_t>(seconds * kNanoSecondsPerSecond);
    if (nanoseconds < kMinInterval)
      nanoseconds = kMinInterval;
    ts.tv_sec = static_cast<time_t>(nanoseconds / kNanoSecondsPerSecond);
    ts.tv_nsec = static_cast<long>(nanoseconds % kNanoSecondsPerSecond);
    return ts;
  }

  EventLoop* loop_;
  const int timerfd_;
  Channel timerfdChannel_;
  const double interval_; // in seconds
  TimerCallback cb_;
};

int main(int argc, char* argv[])
{
	LOG_INFO << "pid = " << getpid() << ", tid = " << CurrentThread::tid()
           	<< " Try adjusting the wall clock, see what happens.";
  	
	EventLoop loop;
  	PeriodicTimer timer(&loop, 1, std::bind(print, "PeriodicTimer"));
  	timer.start();
  
	loop.runEvery(1, std::bind(print, "EventLoop::runEvery"));
  	loop.loop();
}

