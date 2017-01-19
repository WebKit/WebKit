/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contains interfaces for DataChannels
// http://dev.w3.org/2011/webrtc/editor/webrtc.html#rtcdatachannel

#ifndef WEBRTC_API_DATACHANNELINTERFACE_H_
#define WEBRTC_API_DATACHANNELINTERFACE_H_

#include <string>

#include "webrtc/base/basictypes.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/copyonwritebuffer.h"
#include "webrtc/base/refcount.h"


namespace webrtc {

struct DataChannelInit {
  DataChannelInit()
      : reliable(false),
        ordered(true),
        maxRetransmitTime(-1),
        maxRetransmits(-1),
        negotiated(false),
        id(-1) {
  }

  bool reliable;           // Deprecated.
  bool ordered;            // True if ordered delivery is required.
  int maxRetransmitTime;   // The max period of time in milliseconds in which
                           // retransmissions will be sent.  After this time, no
                           // more retransmissions will be sent. -1 if unset.
  int maxRetransmits;      // The max number of retransmissions. -1 if unset.
  std::string protocol;    // This is set by the application and opaque to the
                           // WebRTC implementation.
  bool negotiated;         // True if the channel has been externally negotiated
                           // and we do not send an in-band signalling in the
                           // form of an "open" message.
  int id;                  // The stream id, or SID, for SCTP data channels. -1
                           // if unset.
};

struct DataBuffer {
  DataBuffer(const rtc::CopyOnWriteBuffer& data, bool binary)
      : data(data),
        binary(binary) {
  }
  // For convenience for unit tests.
  explicit DataBuffer(const std::string& text)
      : data(text.data(), text.length()),
        binary(false) {
  }
  size_t size() const { return data.size(); }

  rtc::CopyOnWriteBuffer data;
  // Indicates if the received data contains UTF-8 or binary data.
  // Note that the upper layers are left to verify the UTF-8 encoding.
  // TODO(jiayl): prefer to use an enum instead of a bool.
  bool binary;
};

class DataChannelObserver {
 public:
  // The data channel state have changed.
  virtual void OnStateChange() = 0;
  //  A data buffer was successfully received.
  virtual void OnMessage(const DataBuffer& buffer) = 0;
  // The data channel's buffered_amount has changed.
  virtual void OnBufferedAmountChange(uint64_t){};

 protected:
  virtual ~DataChannelObserver() {}
};

class DataChannelInterface : public rtc::RefCountInterface {
 public:
  // Keep in sync with DataChannel.java:State and
  // RTCDataChannel.h:RTCDataChannelState.
  enum DataState {
    kConnecting,
    kOpen,  // The DataChannel is ready to send data.
    kClosing,
    kClosed
  };

  static const char* DataStateString(DataState state) {
    switch (state) {
      case kConnecting:
        return "connecting";
      case kOpen:
        return "open";
      case kClosing:
        return "closing";
      case kClosed:
        return "closed";
    }
    RTC_CHECK(false) << "Unknown DataChannel state: " << state;
    return "";
  }

  virtual void RegisterObserver(DataChannelObserver* observer) = 0;
  virtual void UnregisterObserver() = 0;
  // The label attribute represents a label that can be used to distinguish this
  // DataChannel object from other DataChannel objects.
  virtual std::string label() const = 0;
  virtual bool reliable() const = 0;

  // TODO(tommyw): Remove these dummy implementations when all classes have
  // implemented these APIs. They should all just return the values the
  // DataChannel was created with.
  virtual bool ordered() const { return false; }
  virtual uint16_t maxRetransmitTime() const { return 0; }
  virtual uint16_t maxRetransmits() const { return 0; }
  virtual std::string protocol() const { return std::string(); }
  virtual bool negotiated() const { return false; }

  virtual int id() const = 0;
  virtual DataState state() const = 0;
  // TODO(hbos): Default implementations of [messages/bytes]_[sent/received] as
  // to not break Chromium. Fix Chromium's implementation as soon as this rolls.
  // crbug.com/654927
  virtual uint32_t messages_sent() const { return 0; }
  virtual uint64_t bytes_sent() const { return 0; }
  virtual uint32_t messages_received() const { return 0; }
  virtual uint64_t bytes_received() const { return 0; }
  // The buffered_amount returns the number of bytes of application data
  // (UTF-8 text and binary data) that have been queued using SendBuffer but
  // have not yet been transmitted to the network.
  virtual uint64_t buffered_amount() const = 0;
  virtual void Close() = 0;
  // Sends |data| to the remote peer.
  virtual bool Send(const DataBuffer& buffer) = 0;

 protected:
  virtual ~DataChannelInterface() {}
};

}  // namespace webrtc

#endif  // WEBRTC_API_DATACHANNELINTERFACE_H_
