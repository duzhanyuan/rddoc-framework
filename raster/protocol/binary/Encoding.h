/*
 * Copyright (C) 2017, Yeolar
 */

#pragma once

#include <string>
#include <arpa/inet.h>
#include "raster/net/Protocol.h"
#include "raster/io/Cursor.h"
#include "raster/io/TypedIOBuf.h"

namespace rdd {
namespace binary {

// buf -> ibuf
inline bool decodeData(IOBuf* buf, ByteRange* ibuf) {
  auto range = buf->coalesce();
  RDDLOG_ON(V4) {
    std::string hex;
    hexlify(range, hex);
    RDDLOG(V4) << "decode binary data: " << hex;
  }
  uint32_t header = *TypedIOBuf<uint32_t>(buf).data();
  RDDLOG(V3) << "decode binary size: " << ntohl(header);
  range.advance(sizeof(uint32_t));
  *ibuf = range;
  return true;
}

// obuf -> buf
inline bool encodeData(IOBuf* buf, ByteRange* obuf) {
  uint8_t* p = (uint8_t*)obuf->data();
  uint32_t n = obuf->size();
  RDDLOG(V3) << "encode binary size: " << n;
  TypedIOBuf<uint32_t>(buf).push(htonl(n));
  rdd::io::Appender appender(buf, Protocol::CHUNK_SIZE);
  appender.pushAtMost(p, n);
  RDDLOG_ON(V4) {
    auto range = buf->coalesce();
    std::string hex;
    hexlify(range, hex);
    RDDLOG(V4) << "encode binary data: " << hex;
  }
  return true;
}

} // namespace binary
} // namespace rdd
