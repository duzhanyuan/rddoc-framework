/*
 * Copyright (C) 2017, Yeolar
 */

#include <gflags/gflags.h>
#include "raster/coroutine/GenericExecutor.h"
#include "raster/framework/Config.h"
#include "raster/net/Actor.h"
#include "raster/parallel/Scheduler.h"
#include "raster/protocol/thrift/AsyncServer.h"
#include "raster/util/Logging.h"
#include "raster/util/ReflectObject.h"
#include "raster/util/ScopeGuard.h"
#include "raster/util/Signal.h"
#include "raster/util/Uuid.h"
#include "gen-cpp/Parallel.h"

static const char* VERSION = "1.0.0";

DEFINE_string(conf, "server.json", "Server config file");

namespace rdd {

void parallelFunc(int id) {
  RDDLOG(INFO) << "handle in parallelFunc " << id;
}

class ParallelJobExecutor
  : public ReflectObject<JobExecutor, ParallelJobExecutor> {
public:
  struct MyContext : public Context {
    parallel::Result* result;
  };

  virtual ~ParallelJobExecutor() {}

  void handle() {
    if (context_) {
      RDDLOG(INFO) << "handle in " << name_ << ": code="
        << std::dynamic_pointer_cast<MyContext>(context_)->result->code;
    } else {
      RDDLOG(INFO) << "handle in " << name_;
    }
  }
};
RDD_RF_REG(JobExecutor, ParallelJobExecutor);

namespace parallel {

class ParallelHandler : virtual public ParallelIf {
public:
  ParallelHandler() {
    RDDLOG(DEBUG) << "ParallelHandler init";
  }

  void run(Result& _return, const Query& query) {
    if (!StringPiece(query.traceid).startsWith("rdd")) {
      _return.__set_code(ResultCode::E_SOURCE__UNTRUSTED);
      RDDLOG(INFO) << "untrusted request: [" << query.traceid << "]";
    }
    if (!checkOK(_return)) return;

    _return.__set_traceid(generateUuid(query.traceid, "rdde"));
    _return.__set_code(ResultCode::OK);

    // function style parallel executing
    auto scheduler = make_unique<Scheduler>();
    for (size_t i = 1; i <= 4; i++) {
      scheduler->add(std::bind(parallelFunc, i));
    }
    scheduler->run();

    // config job style parallel executing
    auto jobctx = new ParallelJobExecutor::MyContext();
    jobctx->result = &_return;
    make_unique<Scheduler>("graph1", JobExecutor::ContextPtr(jobctx))->run();

    RDDTLOG(INFO, query.traceid) << "query: \"" << query.query << "\""
      << " code=" << _return.code;
    if (!checkOK(_return)) return;
  }

private:
  bool checkOK(const Result& result) {
    return result.code < 1000;
  }
};

}
}

using namespace rdd;

int main(int argc, char* argv[]) {
  google::SetVersionString(VERSION);
  google::SetUsageMessage("Usage : ./parallel");
  google::ParseCommandLineFlags(&argc, &argv, true);

  setupIgnoreSignal(SIGPIPE);
  setupShutdownSignal(SIGINT);
  setupShutdownSignal(SIGTERM);

  std::shared_ptr<Service> parallel(
      new TAsyncServer<parallel::ParallelHandler,
                       parallel::ParallelProcessor>());
  Singleton<Actor>::get()->addService("Parallel", parallel);

  config(FLAGS_conf.c_str(), {
         {configLogging, "logging"},
         {configActor, "actor"},
         {configService, "service"},
         {configThreadPool, "thread"},
         {configNetCopy, "net.copy"},
         {configMonitor, "monitor"},
         {configJobGraph, "job.graph"}
         });

  RDDLOG(INFO) << "rdd start ... ^_^";
  Singleton<Actor>::get()->start();

  google::ShutDownCommandLineFlags();

  return 0;
}
