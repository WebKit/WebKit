/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_TESTUTILS_H_
#define RTC_BASE_TESTUTILS_H_

// Utilities for testing rtc infrastructure in unittests

#include <algorithm>
#include <map>
#include <memory>
#include <vector>
#include "rtc_base/asyncsocket.h"
#include "rtc_base/checks.h"
#include "rtc_base/gunit.h"
#include "rtc_base/stream.h"
#include "rtc_base/stringutils.h"

namespace webrtc {
namespace testing {

using namespace rtc;

///////////////////////////////////////////////////////////////////////////////
// StreamSink - Monitor asynchronously signalled events from StreamInterface
// or AsyncSocket (which should probably be a StreamInterface.
///////////////////////////////////////////////////////////////////////////////

// Note: Any event that is an error is treaded as SSE_ERROR instead of that
// event.

enum StreamSinkEvent {
  SSE_OPEN = SE_OPEN,
  SSE_READ = SE_READ,
  SSE_WRITE = SE_WRITE,
  SSE_CLOSE = SE_CLOSE,
  SSE_ERROR = 16
};

class StreamSink : public sigslot::has_slots<> {
 public:
  StreamSink();
  ~StreamSink() override;

  void Monitor(StreamInterface* stream) {
    stream->SignalEvent.connect(this, &StreamSink::OnEvent);
    events_.erase(stream);
  }
  void Unmonitor(StreamInterface* stream) {
    stream->SignalEvent.disconnect(this);
    // In case you forgot to unmonitor a previous object with this address
    events_.erase(stream);
  }
  bool Check(StreamInterface* stream,
             StreamSinkEvent event,
             bool reset = true) {
    return DoCheck(stream, event, reset);
  }
  int Events(StreamInterface* stream, bool reset = true) {
    return DoEvents(stream, reset);
  }

  void Monitor(AsyncSocket* socket) {
    socket->SignalConnectEvent.connect(this, &StreamSink::OnConnectEvent);
    socket->SignalReadEvent.connect(this, &StreamSink::OnReadEvent);
    socket->SignalWriteEvent.connect(this, &StreamSink::OnWriteEvent);
    socket->SignalCloseEvent.connect(this, &StreamSink::OnCloseEvent);
    // In case you forgot to unmonitor a previous object with this address
    events_.erase(socket);
  }
  void Unmonitor(AsyncSocket* socket) {
    socket->SignalConnectEvent.disconnect(this);
    socket->SignalReadEvent.disconnect(this);
    socket->SignalWriteEvent.disconnect(this);
    socket->SignalCloseEvent.disconnect(this);
    events_.erase(socket);
  }
  bool Check(AsyncSocket* socket, StreamSinkEvent event, bool reset = true) {
    return DoCheck(socket, event, reset);
  }
  int Events(AsyncSocket* socket, bool reset = true) {
    return DoEvents(socket, reset);
  }

 private:
  typedef std::map<void*, int> EventMap;

  void OnEvent(StreamInterface* stream, int events, int error) {
    if (error) {
      events = SSE_ERROR;
    }
    AddEvents(stream, events);
  }
  void OnConnectEvent(AsyncSocket* socket) { AddEvents(socket, SSE_OPEN); }
  void OnReadEvent(AsyncSocket* socket) { AddEvents(socket, SSE_READ); }
  void OnWriteEvent(AsyncSocket* socket) { AddEvents(socket, SSE_WRITE); }
  void OnCloseEvent(AsyncSocket* socket, int error) {
    AddEvents(socket, (0 == error) ? SSE_CLOSE : SSE_ERROR);
  }

  void AddEvents(void* obj, int events) {
    EventMap::iterator it = events_.find(obj);
    if (events_.end() == it) {
      events_.insert(EventMap::value_type(obj, events));
    } else {
      it->second |= events;
    }
  }
  bool DoCheck(void* obj, StreamSinkEvent event, bool reset) {
    EventMap::iterator it = events_.find(obj);
    if ((events_.end() == it) || (0 == (it->second & event))) {
      return false;
    }
    if (reset) {
      it->second &= ~event;
    }
    return true;
  }
  int DoEvents(void* obj, bool reset) {
    EventMap::iterator it = events_.find(obj);
    if (events_.end() == it)
      return 0;
    int events = it->second;
    if (reset) {
      it->second = 0;
    }
    return events;
  }

  EventMap events_;
};

///////////////////////////////////////////////////////////////////////////////
// StreamSource - Implements stream interface and simulates asynchronous
// events on the stream, without a network.  Also buffers written data.
///////////////////////////////////////////////////////////////////////////////

class StreamSource : public StreamInterface {
 public:
  StreamSource();
  ~StreamSource() override;

  void Clear() {
    readable_data_.clear();
    written_data_.clear();
    state_ = SS_CLOSED;
    read_block_ = 0;
    write_block_ = SIZE_UNKNOWN;
  }
  void QueueString(const char* data) { QueueData(data, strlen(data)); }
#if defined(__GNUC__)
  // Note: Implicit |this| argument counts as the first argument.
  __attribute__((__format__(__printf__, 2, 3)))
#endif
  void
  QueueStringF(const char* format, ...) {
    va_list args;
    va_start(args, format);
    char buffer[1024];
    size_t len = vsprintfn(buffer, sizeof(buffer), format, args);
    RTC_CHECK(len < sizeof(buffer) - 1);
    va_end(args);
    QueueData(buffer, len);
  }
  void QueueData(const char* data, size_t len) {
    readable_data_.insert(readable_data_.end(), data, data + len);
    if ((SS_OPEN == state_) && (readable_data_.size() == len)) {
      SignalEvent(this, SE_READ, 0);
    }
  }
  std::string ReadData() {
    std::string data;
    // avoid accessing written_data_[0] if it is undefined
    if (written_data_.size() > 0) {
      data.insert(0, &written_data_[0], written_data_.size());
    }
    written_data_.clear();
    return data;
  }
  void SetState(StreamState state) {
    int events = 0;
    if ((SS_OPENING == state_) && (SS_OPEN == state)) {
      events |= SE_OPEN;
      if (!readable_data_.empty()) {
        events |= SE_READ;
      }
    } else if ((SS_CLOSED != state_) && (SS_CLOSED == state)) {
      events |= SE_CLOSE;
    }
    state_ = state;
    if (events) {
      SignalEvent(this, events, 0);
    }
  }
  // Will cause Read to block when there are pos bytes in the read queue.
  void SetReadBlock(size_t pos) { read_block_ = pos; }
  // Will cause Write to block when there are pos bytes in the write queue.
  void SetWriteBlock(size_t pos) { write_block_ = pos; }

  StreamState GetState() const override;
  StreamResult Read(void* buffer,
                    size_t buffer_len,
                    size_t* read,
                    int* error) override;
  StreamResult Write(const void* data,
                     size_t data_len,
                     size_t* written,
                     int* error) override;
  void Close() override;

 private:
  typedef std::vector<char> Buffer;
  Buffer readable_data_, written_data_;
  StreamState state_;
  size_t read_block_, write_block_;
};

}  // namespace testing
}  // namespace webrtc

#endif  // RTC_BASE_TESTUTILS_H_
