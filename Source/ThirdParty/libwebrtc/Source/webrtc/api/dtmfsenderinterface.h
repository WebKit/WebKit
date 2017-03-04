/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_API_DTMFSENDERINTERFACE_H_
#define WEBRTC_API_DTMFSENDERINTERFACE_H_

#include <string>

#include "webrtc/api/mediastreaminterface.h"
#include "webrtc/base/refcount.h"

namespace webrtc {

// DtmfSender callback interface, used to implement RTCDtmfSender events.
// Applications should implement this interface to get notifications from the
// DtmfSender.
class DtmfSenderObserverInterface {
 public:
  // Triggered when DTMF |tone| is sent.
  // If |tone| is empty that means the DtmfSender has sent out all the given
  // tones.
  virtual void OnToneChange(const std::string& tone) = 0;

 protected:
  virtual ~DtmfSenderObserverInterface() {}
};

// The interface of native implementation of the RTCDTMFSender defined by the
// WebRTC W3C Editor's Draft.
// See: https://www.w3.org/TR/webrtc/#peer-to-peer-dtmf
class DtmfSenderInterface : public rtc::RefCountInterface {
 public:
  // Used to receive events from the DTMF sender. Only one observer can be
  // registered at a time. UnregisterObserver should be called before the
  // observer object is destroyed.
  virtual void RegisterObserver(DtmfSenderObserverInterface* observer) = 0;
  virtual void UnregisterObserver() = 0;

  // Returns true if this DtmfSender is capable of sending DTMF. Otherwise
  // returns false. To be able to send DTMF, the associated RtpSender must be
  // able to send packets, and a "telephone-event" codec must be negotiated.
  virtual bool CanInsertDtmf() = 0;

  // Queues a task that sends the DTMF |tones|. The |tones| parameter is treated
  // as a series of characters. The characters 0 through 9, A through D, #, and
  // * generate the associated DTMF tones. The characters a to d are equivalent
  // to A to D. The character ',' indicates a delay of 2 seconds before
  // processing the next character in the tones parameter.
  //
  // Unrecognized characters are ignored.
  //
  // The |duration| parameter indicates the duration in ms to use for each
  // character passed in the |tones| parameter. The duration cannot be more
  // than 6000 or less than 70.
  //
  // The |inter_tone_gap| parameter indicates the gap between tones in ms. The
  // |inter_tone_gap| must be at least 50 ms but should be as short as
  // possible.
  //
  // If InsertDtmf is called on the same object while an existing task for this
  // object to generate DTMF is still running, the previous task is canceled.
  // Returns true on success and false on failure.
  virtual bool InsertDtmf(const std::string& tones, int duration,
                          int inter_tone_gap) = 0;

  // Returns the track given as argument to the constructor. Only exists for
  // backwards compatibilty; now that DtmfSenders are tied to RtpSenders, it's
  // no longer relevant.
  virtual const AudioTrackInterface* track() const = 0;

  // Returns the tones remaining to be played out.
  virtual std::string tones() const = 0;

  // Returns the current tone duration value in ms.
  // This value will be the value last set via the InsertDtmf() method, or the
  // default value of 100 ms if InsertDtmf() was never called.
  virtual int duration() const = 0;

  // Returns the current value of the between-tone gap in ms.
  // This value will be the value last set via the InsertDtmf() method, or the
  // default value of 50 ms if InsertDtmf() was never called.
  virtual int inter_tone_gap() const = 0;

 protected:
  virtual ~DtmfSenderInterface() {}
};

}  // namespace webrtc

#endif  // WEBRTC_API_DTMFSENDERINTERFACE_H_
