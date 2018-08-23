/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contains interfaces for RtpSenders
// http://w3c.github.io/webrtc-pc/#rtcrtpsender-interface

#ifndef API_RTPSENDERINTERFACE_H_
#define API_RTPSENDERINTERFACE_H_

#include <string>
#include <vector>

#include "api/dtmfsenderinterface.h"
#include "api/mediastreaminterface.h"
#include "api/mediatypes.h"
#include "api/proxy.h"
#include "api/rtcerror.h"
#include "api/rtpparameters.h"
#include "rtc_base/deprecation.h"
#include "rtc_base/refcount.h"
#include "rtc_base/scoped_ref_ptr.h"

namespace webrtc {

class RtpSenderInterface : public rtc::RefCountInterface {
 public:
  // Returns true if successful in setting the track.
  // Fails if an audio track is set on a video RtpSender, or vice-versa.
  virtual bool SetTrack(MediaStreamTrackInterface* track) = 0;
  virtual rtc::scoped_refptr<MediaStreamTrackInterface> track() const = 0;

  // Returns primary SSRC used by this sender for sending media.
  // Returns 0 if not yet determined.
  // TODO(deadbeef): Change to absl::optional.
  // TODO(deadbeef): Remove? With GetParameters this should be redundant.
  virtual uint32_t ssrc() const = 0;

  // Audio or video sender?
  virtual cricket::MediaType media_type() const = 0;

  // Not to be confused with "mid", this is a field we can temporarily use
  // to uniquely identify a receiver until we implement Unified Plan SDP.
  virtual std::string id() const = 0;

  // Returns a list of media stream ids associated with this sender's track.
  // These are signalled in the SDP so that the remote side can associate
  // tracks.
  virtual std::vector<std::string> stream_ids() const = 0;

  virtual RtpParameters GetParameters() = 0;
  // Note that only a subset of the parameters can currently be changed. See
  // rtpparameters.h
  // The encodings are in increasing quality order for simulcast.
  virtual RTCError SetParameters(const RtpParameters& parameters) = 0;

  // Returns null for a video sender.
  virtual rtc::scoped_refptr<DtmfSenderInterface> GetDtmfSender() const = 0;

 protected:
  ~RtpSenderInterface() override = default;
};

// Define proxy for RtpSenderInterface.
// TODO(deadbeef): Move this to .cc file and out of api/. What threads methods
// are called on is an implementation detail.
BEGIN_SIGNALING_PROXY_MAP(RtpSender)
PROXY_SIGNALING_THREAD_DESTRUCTOR()
PROXY_METHOD1(bool, SetTrack, MediaStreamTrackInterface*)
PROXY_CONSTMETHOD0(rtc::scoped_refptr<MediaStreamTrackInterface>, track)
PROXY_CONSTMETHOD0(uint32_t, ssrc)
PROXY_CONSTMETHOD0(cricket::MediaType, media_type)
PROXY_CONSTMETHOD0(std::string, id)
PROXY_CONSTMETHOD0(std::vector<std::string>, stream_ids)
PROXY_METHOD0(RtpParameters, GetParameters);
PROXY_METHOD1(RTCError, SetParameters, const RtpParameters&)
PROXY_CONSTMETHOD0(rtc::scoped_refptr<DtmfSenderInterface>, GetDtmfSender);
END_PROXY_MAP()

}  // namespace webrtc

#endif  // API_RTPSENDERINTERFACE_H_
