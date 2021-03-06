/*
 * Copyright (C) 2017, Yeolar
 */

#pragma once

#include <atomic>
#include <assert.h>
#include "raster/coroutine/BoostContext.h"
#include "raster/coroutine/Executor.h"
#include "raster/util/Logging.h"
#include "raster/util/noncopyable.h"
#include "raster/util/Time.h"

#define RDD_FCLOG(severity, t) \
  RDDLOG(severity) << "fiber(" << (void*)(t) << ", " \
    << (t)->timestampStr() << ") "

namespace rdd {

class Fiber : noncopyable {
public:
  enum {
    INIT,     // I
    RUNABLE,  // A
    RUNNING,  // R
    BLOCK,    // B
    EXIT,     // Q
  };

  static size_t count() { return count_; }

  Fiber(int stackSize, const ExecutorPtr& executor)
    : stackLimit_(new unsigned char[stackSize]),
      stackSize_(stackSize),
      context_(std::bind(&Executor::run, executor.get()),
               stackLimit_, stackSize_),
      qstart_(timestampNow()) {
    executor_ = executor;
    executor_->fiber = this;
    timestamps_.emplace_back(Timestamp(status_));
    ++count_;
  }

  virtual ~Fiber() {
    delete stackLimit_;
    --count_;
  }

  ExecutorPtr executor() const { return executor_; }

  int status() const { return status_; }

  void setStatus(int status) {
    status_ = status;
    record(Timestamp(status_));
  }
  char statusLabel() const {
    return "IARBQW"[status_];
  }

  bool isTimeout() {
    if (qtimeout_ > 0) {
      uint64_t interval = timePassed(qstart_);
      if (interval > qtimeout_) {
        RDD_FCLOG(WARN, this) << "is timeout(us): "
          << interval << ">" << qtimeout_;
        return true;
      }
    }
    return false;
  }

  void execute() {
    assert(status_ == RUNABLE);
    setStatus(RUNNING);
    RDD_FCLOG(V5, this) << "execute";
    context_.activate();
  }
  void yield(int status) {
    assert(status_ == RUNNING);
    setStatus(status);
    RDD_FCLOG(V5, this) << "yield:" << statusLabel();
    context_.deactivate();
  }

  uint64_t starttime() const {
    return timestamps_.empty() ? 0 : timestamps_.front().stamp;
  }
  void record(Timestamp timestamp) {
    if (!timestamps_.empty()) {
      timestamp.stamp -= starttime();
    }
    timestamps_.emplace_back(timestamp);
  }
  uint64_t cost() const {
    return timestamps_.empty() ? 0 : timePassed(starttime());
  }

  std::string timestampStr() const {
    return join("-", timestamps_);
  }

private:
  static std::atomic<size_t> count_;

  int status_{INIT};
  unsigned char* stackLimit_;
  size_t stackSize_;
  Context context_;
  ExecutorPtr executor_;

  uint64_t qstart_;           // deq start
  uint64_t qtimeout_{300000}; // deq timeout
  std::vector<Timestamp> timestamps_;
};

} // namespace rdd
