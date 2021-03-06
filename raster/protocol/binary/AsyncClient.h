/*
 * Copyright (C) 2017, Yeolar
 */

#pragma once

#include "raster/net/AsyncClient.h"
#include "raster/protocol/binary/Encoding.h"
#include "raster/protocol/binary/Protocol.h"

namespace rdd {

class BinaryAsyncClient : public AsyncClient {
public:
  BinaryAsyncClient(const ClientOption& option)
    : AsyncClient(option) {
    channel_ = makeChannel();
  }
  BinaryAsyncClient(const std::string& host,
               int port,
               uint64_t ctimeout = 100000,
               uint64_t rtimeout = 1000000,
               uint64_t wtimeout = 300000)
    : BinaryAsyncClient({Peer(host, port), {ctimeout, rtimeout, wtimeout}}) {
  }
  virtual ~BinaryAsyncClient() {}

  template <class Res = ByteRange>
  bool recv(Res& _return) {
    if (!event_ || event_->type() == Event::FAIL) {
      return false;
    }
    binary::decodeData(event_->rbuf(), &_return);
    return true;
  }

  template <class Req = ByteRange>
  bool send(const Req& request) {
    if (!event_) {
      return false;
    }
    binary::encodeData(event_->wbuf(), (Req*)&request);
    return true;
  }

  template <class Req = ByteRange, class Res = ByteRange>
  bool fetch(Res& _return, const Req& request) {
    return (send(request) &&
            FiberManager::yield() &&
            recv(_return));
  }

  template <class Req = ByteRange>
  bool fetchNoWait(const Req& request) {
    if (send(request)) {
      Singleton<Actor>::get()->execute((AsyncClient*)this);
      return true;
    }
    return false;
  }

protected:
  virtual std::shared_ptr<Channel> makeChannel() {
    std::shared_ptr<Protocol> protocol(new BinaryProtocol());
    return std::make_shared<Channel>(peer_, timeoutOpt_, protocol);
  }
};

} // namespace rdd
