/**
 * Autogenerated by Thrift Compiler (0.9.1)
 *
 * DO NOT EDIT UNLESS YOU ARE SURE THAT YOU KNOW WHAT YOU ARE DOING
 *  @generated
 */
#ifndef Empty_H
#define Empty_H

#include <thrift/TDispatchProcessor.h>
#include "Empty_types.h"

namespace rdd { namespace empty {

class EmptyIf {
 public:
  virtual ~EmptyIf() {}
  virtual void run(Result& _return, const Query& query) = 0;
};

class EmptyIfFactory {
 public:
  typedef EmptyIf Handler;

  virtual ~EmptyIfFactory() {}

  virtual EmptyIf* getHandler(const ::apache::thrift::TConnectionInfo& connInfo) = 0;
  virtual void releaseHandler(EmptyIf* /* handler */) = 0;
};

class EmptyIfSingletonFactory : virtual public EmptyIfFactory {
 public:
  EmptyIfSingletonFactory(const boost::shared_ptr<EmptyIf>& iface) : iface_(iface) {}
  virtual ~EmptyIfSingletonFactory() {}

  virtual EmptyIf* getHandler(const ::apache::thrift::TConnectionInfo&) {
    return iface_.get();
  }
  virtual void releaseHandler(EmptyIf* /* handler */) {}

 protected:
  boost::shared_ptr<EmptyIf> iface_;
};

class EmptyNull : virtual public EmptyIf {
 public:
  virtual ~EmptyNull() {}
  void run(Result& /* _return */, const Query& /* query */) {
    return;
  }
};

typedef struct _Empty_run_args__isset {
  _Empty_run_args__isset() : query(false) {}
  bool query;
} _Empty_run_args__isset;

class Empty_run_args {
 public:

  Empty_run_args() {
  }

  virtual ~Empty_run_args() throw() {}

  Query query;

  _Empty_run_args__isset __isset;

  void __set_query(const Query& val) {
    query = val;
  }

  bool operator == (const Empty_run_args & rhs) const
  {
    if (!(query == rhs.query))
      return false;
    return true;
  }
  bool operator != (const Empty_run_args &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const Empty_run_args & ) const;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

};


class Empty_run_pargs {
 public:


  virtual ~Empty_run_pargs() throw() {}

  const Query* query;

  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

};

typedef struct _Empty_run_result__isset {
  _Empty_run_result__isset() : success(false) {}
  bool success;
} _Empty_run_result__isset;

class Empty_run_result {
 public:

  Empty_run_result() {
  }

  virtual ~Empty_run_result() throw() {}

  Result success;

  _Empty_run_result__isset __isset;

  void __set_success(const Result& val) {
    success = val;
  }

  bool operator == (const Empty_run_result & rhs) const
  {
    if (!(success == rhs.success))
      return false;
    return true;
  }
  bool operator != (const Empty_run_result &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const Empty_run_result & ) const;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

};

typedef struct _Empty_run_presult__isset {
  _Empty_run_presult__isset() : success(false) {}
  bool success;
} _Empty_run_presult__isset;

class Empty_run_presult {
 public:


  virtual ~Empty_run_presult() throw() {}

  Result* success;

  _Empty_run_presult__isset __isset;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);

};

class EmptyClient : virtual public EmptyIf {
 public:
  EmptyClient(boost::shared_ptr< ::apache::thrift::protocol::TProtocol> prot) :
    piprot_(prot),
    poprot_(prot) {
    iprot_ = prot.get();
    oprot_ = prot.get();
  }
  EmptyClient(boost::shared_ptr< ::apache::thrift::protocol::TProtocol> iprot, boost::shared_ptr< ::apache::thrift::protocol::TProtocol> oprot) :
    piprot_(iprot),
    poprot_(oprot) {
    iprot_ = iprot.get();
    oprot_ = oprot.get();
  }
  boost::shared_ptr< ::apache::thrift::protocol::TProtocol> getInputProtocol() {
    return piprot_;
  }
  boost::shared_ptr< ::apache::thrift::protocol::TProtocol> getOutputProtocol() {
    return poprot_;
  }
  void run(Result& _return, const Query& query);
  void send_run(const Query& query);
  void recv_run(Result& _return);
 protected:
  boost::shared_ptr< ::apache::thrift::protocol::TProtocol> piprot_;
  boost::shared_ptr< ::apache::thrift::protocol::TProtocol> poprot_;
  ::apache::thrift::protocol::TProtocol* iprot_;
  ::apache::thrift::protocol::TProtocol* oprot_;
};

class EmptyProcessor : public ::apache::thrift::TDispatchProcessor {
 protected:
  boost::shared_ptr<EmptyIf> iface_;
  virtual bool dispatchCall(::apache::thrift::protocol::TProtocol* iprot, ::apache::thrift::protocol::TProtocol* oprot, const std::string& fname, int32_t seqid, void* callContext);
 private:
  typedef  void (EmptyProcessor::*ProcessFunction)(int32_t, ::apache::thrift::protocol::TProtocol*, ::apache::thrift::protocol::TProtocol*, void*);
  typedef std::map<std::string, ProcessFunction> ProcessMap;
  ProcessMap processMap_;
  void process_run(int32_t seqid, ::apache::thrift::protocol::TProtocol* iprot, ::apache::thrift::protocol::TProtocol* oprot, void* callContext);
 public:
  EmptyProcessor(boost::shared_ptr<EmptyIf> iface) :
    iface_(iface) {
    processMap_["run"] = &EmptyProcessor::process_run;
  }

  virtual ~EmptyProcessor() {}
};

class EmptyProcessorFactory : public ::apache::thrift::TProcessorFactory {
 public:
  EmptyProcessorFactory(const ::boost::shared_ptr< EmptyIfFactory >& handlerFactory) :
      handlerFactory_(handlerFactory) {}

  ::boost::shared_ptr< ::apache::thrift::TProcessor > getProcessor(const ::apache::thrift::TConnectionInfo& connInfo);

 protected:
  ::boost::shared_ptr< EmptyIfFactory > handlerFactory_;
};

class EmptyMultiface : virtual public EmptyIf {
 public:
  EmptyMultiface(std::vector<boost::shared_ptr<EmptyIf> >& ifaces) : ifaces_(ifaces) {
  }
  virtual ~EmptyMultiface() {}
 protected:
  std::vector<boost::shared_ptr<EmptyIf> > ifaces_;
  EmptyMultiface() {}
  void add(boost::shared_ptr<EmptyIf> iface) {
    ifaces_.push_back(iface);
  }
 public:
  void run(Result& _return, const Query& query) {
    size_t sz = ifaces_.size();
    size_t i = 0;
    for (; i < (sz - 1); ++i) {
      ifaces_[i]->run(_return, query);
    }
    ifaces_[i]->run(_return, query);
    return;
  }

};

}} // namespace

#endif
