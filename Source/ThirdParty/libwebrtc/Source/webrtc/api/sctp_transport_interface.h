/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_SCTP_TRANSPORT_INTERFACE_H_
#define API_SCTP_TRANSPORT_INTERFACE_H_

#include <cstdint>
#include <optional>
#include <vector>

#include "api/dtls_transport_interface.h"
#include "api/ref_count.h"
#include "api/scoped_refptr.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

// Constants that are important to API users

// The number of outgoing streams that we'll negotiate. Since stream IDs (SIDs)
// are 0-based, the highest usable SID is 1023.
//
// It's recommended to use the maximum of 65535 in:
// https://tools.ietf.org/html/draft-ietf-rtcweb-data-channel-13#section-6.2
// However, we use 1024 in order to save memory. usrsctp allocates 104 bytes
// for each pair of incoming/outgoing streams (on a 64-bit system), so 65535
// streams would waste ~6MB.
//
// Note: "max" and "min" here are inclusive.
constexpr uint16_t kMaxSctpStreams = 1024;
constexpr uint16_t kMaxSctpSid = kMaxSctpStreams - 1;
constexpr uint16_t kMinSctpSid = 0;
// The maximum number of streams that can be negotiated according to spec.
constexpr uint16_t kSpecMaxSctpSid = 65535;

// This is the default SCTP port to use. It is passed along the wire and the
// connectee and connector must be using the same port. It is not related to the
// ports at the IP level. (Corresponds to: sockaddr_conn.sconn_port in
// usrsctp.h)
const int kSctpDefaultPort = 5000;

// Error cause codes defined at
// https://www.iana.org/assignments/sctp-parameters/sctp-parameters.xhtml#sctp-parameters-24
enum class SctpErrorCauseCode : uint16_t {
  kInvalidStreamIdentifier = 1,
  kMissingMandatoryParameter = 2,
  kStaleCookieError = 3,
  kOutOfResource = 4,
  kUnresolvableAddress = 5,
  kUnrecognizedChunkType = 6,
  kInvalidMandatoryParameter = 7,
  kUnrecognizedParameters = 8,
  kNoUserData = 9,
  kCookieReceivedWhileShuttingDown = 10,
  kRestartWithNewAddresses = 11,
  kUserInitiatedAbort = 12,
  kProtocolViolation = 13,
};

// States of a SCTP transport, corresponding to the JS API specification.
// http://w3c.github.io/webrtc-pc/#dom-rtcsctptransportstate
enum class SctpTransportState {
  kNew,         // Has not started negotiating yet. Non-standard state.
  kConnecting,  // In the process of negotiating an association.
  kConnected,   // Completed negotiation of an association.
  kClosed,      // Closed by local or remote party.
  kNumValues
};

// This object gives snapshot information about the changeable state of a
// SctpTransport.
// It reflects the readonly attributes of the object in the specification.
// http://w3c.github.io/webrtc-pc/#rtcsctptransport-interface
class RTC_EXPORT SctpTransportInformation {
 public:
  SctpTransportInformation() = default;
  SctpTransportInformation(const SctpTransportInformation&) = default;
  explicit SctpTransportInformation(SctpTransportState state);
  SctpTransportInformation(SctpTransportState state,
                           scoped_refptr<DtlsTransportInterface> dtls_transport,
                           std::optional<double> max_message_size,
                           std::optional<int> max_channels);
  ~SctpTransportInformation();
  // The DTLS transport that supports this SCTP transport.
  scoped_refptr<DtlsTransportInterface> dtls_transport() const {
    return dtls_transport_;
  }
  SctpTransportState state() const { return state_; }
  std::optional<double> MaxMessageSize() const { return max_message_size_; }
  std::optional<int> MaxChannels() const { return max_channels_; }

 private:
  SctpTransportState state_ = SctpTransportState::kNew;
  scoped_refptr<DtlsTransportInterface> dtls_transport_;
  std::optional<double> max_message_size_;
  std::optional<int> max_channels_;
};

class SctpTransportObserverInterface {
 public:
  // This callback carries information about the state of the transport.
  // The argument is a pass-by-value snapshot of the state.
  // The callback will be called on the network thread.
  virtual void OnStateChange(SctpTransportInformation info) = 0;

 protected:
  virtual ~SctpTransportObserverInterface() = default;
};

// A SCTP transport, as represented to the outside world.
// This object is created on the network thread, and can only be
// accessed on that thread, except for functions explicitly marked otherwise.
// References can be held by other threads, and destruction can therefore
// be initiated by other threads.
class SctpTransportInterface : public RefCountInterface {
 public:
  // This function can be called from other threads.
  virtual scoped_refptr<DtlsTransportInterface> dtls_transport() const = 0;
  // Returns information on the state of the SctpTransport.
  // This function can be called from other threads.
  virtual SctpTransportInformation Information() const = 0;
  // Observer management.
  virtual void RegisterObserver(SctpTransportObserverInterface* observer) = 0;
  virtual void UnregisterObserver() = 0;
};

// The size of the SCTP association send buffer. 256kB, the usrsctp default.
constexpr int kSctpSendBufferSize = 256 * 1024;

// SCTP options negotiated in the SDP.
struct SctpOptions {
  // https://www.rfc-editor.org/rfc/rfc8841.html#name-sctp-port
  // `local_port` and `remote_port` are passed along the wire and the
  // listener and connector must be using the same port. They are not related
  // to the ports at the IP level. If set to -1 we default to
  // kSctpDefaultPort.
  // TODO(bugs.webrtc.org/402429107): make these optional<uint16_t>.
  int local_port = -1;
  int remote_port = -1;

  // https://www.rfc-editor.org/rfc/rfc8841.html#name-max-message-size
  // `max_message_size` sets the maxium message size on the connection.
  // It must be smaller than or equal to kSctpSendBufferSize.
  int max_message_size = kSctpSendBufferSize;

  // draft-hancke-tsvwg-snap
  std::optional<std::vector<uint8_t>> local_init;
  std::optional<std::vector<uint8_t>> remote_init;
};

}  // namespace webrtc

#endif  // API_SCTP_TRANSPORT_INTERFACE_H_
