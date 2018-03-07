/*
 *  Copyright 2009 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_SRTPFILTER_H_
#define PC_SRTPFILTER_H_

#include <list>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "api/cryptoparams.h"
#include "api/optional.h"
#include "p2p/base/sessiondescription.h"
#include "rtc_base/basictypes.h"
#include "rtc_base/buffer.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/sslstreamadapter.h"
#include "rtc_base/thread_checker.h"

// Forward declaration to avoid pulling in libsrtp headers here
struct srtp_event_data_t;
struct srtp_ctx_t_;

namespace cricket {

// A helper class used to negotiate SDES crypto params.
// TODO(zhihuang): Find a better name for this class, like "SdesNegotiator".
class SrtpFilter {
 public:
  enum Mode {
    PROTECT,
    UNPROTECT
  };
  enum Error {
    ERROR_NONE,
    ERROR_FAIL,
    ERROR_AUTH,
    ERROR_REPLAY,
  };

  SrtpFilter();
  ~SrtpFilter();

  // Whether the filter is active (i.e. crypto has been properly negotiated).
  bool IsActive() const;

  // Indicates which crypto algorithms and keys were contained in the offer.
  // offer_params should contain a list of available parameters to use, or none,
  // if crypto is not desired. This must be called before SetAnswer.
  bool SetOffer(const std::vector<CryptoParams>& offer_params,
                ContentSource source);
  // Same as SetAnwer. But multiple calls are allowed to SetProvisionalAnswer
  // after a call to SetOffer.
  bool SetProvisionalAnswer(const std::vector<CryptoParams>& answer_params,
                            ContentSource source);
  // Indicates which crypto algorithms and keys were contained in the answer.
  // answer_params should contain the negotiated parameters, which may be none,
  // if crypto was not desired or could not be negotiated (and not required).
  // This must be called after SetOffer. If crypto negotiation completes
  // successfully, this will advance the filter to the active state.
  bool SetAnswer(const std::vector<CryptoParams>& answer_params,
                 ContentSource source);

  bool ResetParams();

  rtc::Optional<int> send_cipher_suite() { return send_cipher_suite_; }
  rtc::Optional<int> recv_cipher_suite() { return recv_cipher_suite_; }

  const rtc::Buffer& send_key() { return send_key_; }
  const rtc::Buffer& recv_key() { return recv_key_; }

 protected:
  bool ExpectOffer(ContentSource source);

  bool StoreParams(const std::vector<CryptoParams>& params,
                   ContentSource source);

  bool ExpectAnswer(ContentSource source);

  bool DoSetAnswer(const std::vector<CryptoParams>& answer_params,
                   ContentSource source,
                   bool final);

  bool NegotiateParams(const std::vector<CryptoParams>& answer_params,
                       CryptoParams* selected_params);

 private:
  bool ApplySendParams(const CryptoParams& send_params);

  bool ApplyRecvParams(const CryptoParams& recv_params);

  static bool ParseKeyParams(const std::string& params,
                             uint8_t* key,
                             size_t len);

  enum State {
    ST_INIT,           // SRTP filter unused.
    ST_SENTOFFER,      // Offer with SRTP parameters sent.
    ST_RECEIVEDOFFER,  // Offer with SRTP parameters received.
    ST_SENTPRANSWER_NO_CRYPTO,  // Sent provisional answer without crypto.
    // Received provisional answer without crypto.
    ST_RECEIVEDPRANSWER_NO_CRYPTO,
    ST_ACTIVE,         // Offer and answer set.
    // SRTP filter is active but new parameters are offered.
    // When the answer is set, the state transitions to ST_ACTIVE or ST_INIT.
    ST_SENTUPDATEDOFFER,
    // SRTP filter is active but new parameters are received.
    // When the answer is set, the state transitions back to ST_ACTIVE.
    ST_RECEIVEDUPDATEDOFFER,
    // SRTP filter is active but the sent answer is only provisional.
    // When the final answer is set, the state transitions to ST_ACTIVE or
    // ST_INIT.
    ST_SENTPRANSWER,
    // SRTP filter is active but the received answer is only provisional.
    // When the final answer is set, the state transitions to ST_ACTIVE or
    // ST_INIT.
    ST_RECEIVEDPRANSWER
  };
  State state_ = ST_INIT;
  std::vector<CryptoParams> offer_params_;
  CryptoParams applied_send_params_;
  CryptoParams applied_recv_params_;
  rtc::Optional<int> send_cipher_suite_;
  rtc::Optional<int> recv_cipher_suite_;
  rtc::Buffer send_key_;
  rtc::Buffer recv_key_;
};

}  // namespace cricket

#endif  // PC_SRTPFILTER_H_
