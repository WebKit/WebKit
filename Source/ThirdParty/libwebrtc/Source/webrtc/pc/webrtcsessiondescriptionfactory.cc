/*
 *  Copyright 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/pc/webrtcsessiondescriptionfactory.h"

#include <utility>

#include "webrtc/api/jsep.h"
#include "webrtc/api/jsepsessiondescription.h"
#include "webrtc/api/mediaconstraintsinterface.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/sslidentity.h"
#include "webrtc/pc/webrtcsession.h"

using cricket::MediaSessionOptions;

namespace webrtc {
namespace {
static const char kFailedDueToIdentityFailed[] =
    " failed because DTLS identity request failed";
static const char kFailedDueToSessionShutdown[] =
    " failed because the session was shut down";

static const uint64_t kInitSessionVersion = 2;

static bool CompareStream(const MediaSessionOptions::Stream& stream1,
                          const MediaSessionOptions::Stream& stream2) {
  return stream1.id < stream2.id;
}

static bool SameId(const MediaSessionOptions::Stream& stream1,
                   const MediaSessionOptions::Stream& stream2) {
  return stream1.id == stream2.id;
}

// Checks if each Stream within the |streams| has unique id.
static bool ValidStreams(const MediaSessionOptions::Streams& streams) {
  MediaSessionOptions::Streams sorted_streams = streams;
  std::sort(sorted_streams.begin(), sorted_streams.end(), CompareStream);
  MediaSessionOptions::Streams::iterator it =
      std::adjacent_find(sorted_streams.begin(), sorted_streams.end(),
                         SameId);
  return it == sorted_streams.end();
}

enum {
  MSG_CREATE_SESSIONDESCRIPTION_SUCCESS,
  MSG_CREATE_SESSIONDESCRIPTION_FAILED,
  MSG_USE_CONSTRUCTOR_CERTIFICATE
};

struct CreateSessionDescriptionMsg : public rtc::MessageData {
  explicit CreateSessionDescriptionMsg(
      webrtc::CreateSessionDescriptionObserver* observer)
      : observer(observer) {
  }

  rtc::scoped_refptr<webrtc::CreateSessionDescriptionObserver> observer;
  std::string error;
  std::unique_ptr<webrtc::SessionDescriptionInterface> description;
};
}  // namespace

void WebRtcCertificateGeneratorCallback::OnFailure() {
  SignalRequestFailed();
}

void WebRtcCertificateGeneratorCallback::OnSuccess(
    const rtc::scoped_refptr<rtc::RTCCertificate>& certificate) {
  SignalCertificateReady(certificate);
}

// static
void WebRtcSessionDescriptionFactory::CopyCandidatesFromSessionDescription(
    const SessionDescriptionInterface* source_desc,
    const std::string& content_name,
    SessionDescriptionInterface* dest_desc) {
  if (!source_desc) {
    return;
  }
  const cricket::ContentInfos& contents =
      source_desc->description()->contents();
  const cricket::ContentInfo* cinfo =
      source_desc->description()->GetContentByName(content_name);
  if (!cinfo) {
    return;
  }
  size_t mediasection_index = static_cast<int>(cinfo - &contents[0]);
  const IceCandidateCollection* source_candidates =
      source_desc->candidates(mediasection_index);
  const IceCandidateCollection* dest_candidates =
      dest_desc->candidates(mediasection_index);
  if (!source_candidates || !dest_candidates) {
    return;
  }
  for (size_t n = 0; n < source_candidates->count(); ++n) {
    const IceCandidateInterface* new_candidate = source_candidates->at(n);
    if (!dest_candidates->HasCandidate(new_candidate)) {
      dest_desc->AddCandidate(source_candidates->at(n));
    }
  }
}

// Private constructor called by other constructors.
WebRtcSessionDescriptionFactory::WebRtcSessionDescriptionFactory(
    rtc::Thread* signaling_thread,
    cricket::ChannelManager* channel_manager,
    WebRtcSession* session,
    const std::string& session_id,
    std::unique_ptr<rtc::RTCCertificateGeneratorInterface> cert_generator,
    const rtc::scoped_refptr<rtc::RTCCertificate>& certificate)
    : signaling_thread_(signaling_thread),
      session_desc_factory_(channel_manager, &transport_desc_factory_),
      // RFC 4566 suggested a Network Time Protocol (NTP) format timestamp
      // as the session id and session version. To simplify, it should be fine
      // to just use a random number as session id and start version from
      // |kInitSessionVersion|.
      session_version_(kInitSessionVersion),
      cert_generator_(std::move(cert_generator)),
      session_(session),
      session_id_(session_id),
      certificate_request_state_(CERTIFICATE_NOT_NEEDED) {
  RTC_DCHECK(signaling_thread_);
  session_desc_factory_.set_add_legacy_streams(false);
  bool dtls_enabled = cert_generator_ || certificate;
  // SRTP-SDES is disabled if DTLS is on.
  SetSdesPolicy(dtls_enabled ? cricket::SEC_DISABLED : cricket::SEC_REQUIRED);
  if (!dtls_enabled) {
    LOG(LS_VERBOSE) << "DTLS-SRTP disabled.";
    return;
  }

  if (certificate) {
    // Use |certificate|.
    certificate_request_state_ = CERTIFICATE_WAITING;

    LOG(LS_VERBOSE) << "DTLS-SRTP enabled; has certificate parameter.";
    // We already have a certificate but we wait to do |SetIdentity|; if we do
    // it in the constructor then the caller has not had a chance to connect to
    // |SignalCertificateReady|.
    signaling_thread_->Post(
        RTC_FROM_HERE, this, MSG_USE_CONSTRUCTOR_CERTIFICATE,
        new rtc::ScopedRefMessageData<rtc::RTCCertificate>(certificate));
  } else {
    // Generate certificate.
    certificate_request_state_ = CERTIFICATE_WAITING;

    rtc::scoped_refptr<WebRtcCertificateGeneratorCallback> callback(
        new rtc::RefCountedObject<WebRtcCertificateGeneratorCallback>());
    callback->SignalRequestFailed.connect(
        this, &WebRtcSessionDescriptionFactory::OnCertificateRequestFailed);
    callback->SignalCertificateReady.connect(
        this, &WebRtcSessionDescriptionFactory::SetCertificate);

    rtc::KeyParams key_params = rtc::KeyParams();
    LOG(LS_VERBOSE) << "DTLS-SRTP enabled; sending DTLS identity request (key "
                    << "type: " << key_params.type() << ").";

    // Request certificate. This happens asynchronously, so that the caller gets
    // a chance to connect to |SignalCertificateReady|.
    cert_generator_->GenerateCertificateAsync(
        key_params, rtc::Optional<uint64_t>(), callback);
  }
}

WebRtcSessionDescriptionFactory::WebRtcSessionDescriptionFactory(
    rtc::Thread* signaling_thread,
    cricket::ChannelManager* channel_manager,
    WebRtcSession* session,
    const std::string& session_id,
    std::unique_ptr<rtc::RTCCertificateGeneratorInterface> cert_generator)
    : WebRtcSessionDescriptionFactory(
          signaling_thread,
          channel_manager,
          session,
          session_id,
          std::move(cert_generator),
          nullptr) {
}

WebRtcSessionDescriptionFactory::WebRtcSessionDescriptionFactory(
    rtc::Thread* signaling_thread,
    cricket::ChannelManager* channel_manager,
    WebRtcSession* session,
    const std::string& session_id,
    const rtc::scoped_refptr<rtc::RTCCertificate>& certificate)
    : WebRtcSessionDescriptionFactory(signaling_thread,
                                      channel_manager,
                                      session,
                                      session_id,
                                      nullptr,
                                      certificate) {
  RTC_DCHECK(certificate);
}

WebRtcSessionDescriptionFactory::~WebRtcSessionDescriptionFactory() {
  RTC_DCHECK(signaling_thread_->IsCurrent());

  // Fail any requests that were asked for before identity generation completed.
  FailPendingRequests(kFailedDueToSessionShutdown);

  // Process all pending notifications in the message queue.  If we don't do
  // this, requests will linger and not know they succeeded or failed.
  rtc::MessageList list;
  signaling_thread_->Clear(this, rtc::MQID_ANY, &list);
  for (auto& msg : list) {
    if (msg.message_id != MSG_USE_CONSTRUCTOR_CERTIFICATE) {
      OnMessage(&msg);
    } else {
      // Skip MSG_USE_CONSTRUCTOR_CERTIFICATE because we don't want to trigger
      // SetIdentity-related callbacks in the destructor. This can be a problem
      // when WebRtcSession listens to the callback but it was the WebRtcSession
      // destructor that caused WebRtcSessionDescriptionFactory's destruction.
      // The callback is then ignored, leaking memory allocated by OnMessage for
      // MSG_USE_CONSTRUCTOR_CERTIFICATE.
      delete msg.pdata;
    }
  }
}

void WebRtcSessionDescriptionFactory::CreateOffer(
    CreateSessionDescriptionObserver* observer,
    const PeerConnectionInterface::RTCOfferAnswerOptions& options,
    const cricket::MediaSessionOptions& session_options) {
  std::string error = "CreateOffer";
  if (certificate_request_state_ == CERTIFICATE_FAILED) {
    error += kFailedDueToIdentityFailed;
    LOG(LS_ERROR) << error;
    PostCreateSessionDescriptionFailed(observer, error);
    return;
  }

  if (!ValidStreams(session_options.streams)) {
    error += " called with invalid media streams.";
    LOG(LS_ERROR) << error;
    PostCreateSessionDescriptionFailed(observer, error);
    return;
  }

  CreateSessionDescriptionRequest request(
      CreateSessionDescriptionRequest::kOffer, observer, session_options);
  if (certificate_request_state_ == CERTIFICATE_WAITING) {
    create_session_description_requests_.push(request);
  } else {
    RTC_DCHECK(certificate_request_state_ == CERTIFICATE_SUCCEEDED ||
               certificate_request_state_ == CERTIFICATE_NOT_NEEDED);
    InternalCreateOffer(request);
  }
}

void WebRtcSessionDescriptionFactory::CreateAnswer(
    CreateSessionDescriptionObserver* observer,
    const cricket::MediaSessionOptions& session_options) {
  std::string error = "CreateAnswer";
  if (certificate_request_state_ == CERTIFICATE_FAILED) {
    error += kFailedDueToIdentityFailed;
    LOG(LS_ERROR) << error;
    PostCreateSessionDescriptionFailed(observer, error);
    return;
  }
  if (!session_->remote_description()) {
    error += " can't be called before SetRemoteDescription.";
    LOG(LS_ERROR) << error;
    PostCreateSessionDescriptionFailed(observer, error);
    return;
  }
  if (session_->remote_description()->type() !=
      JsepSessionDescription::kOffer) {
    error += " failed because remote_description is not an offer.";
    LOG(LS_ERROR) << error;
    PostCreateSessionDescriptionFailed(observer, error);
    return;
  }

  if (!ValidStreams(session_options.streams)) {
    error += " called with invalid media streams.";
    LOG(LS_ERROR) << error;
    PostCreateSessionDescriptionFailed(observer, error);
    return;
  }

  CreateSessionDescriptionRequest request(
      CreateSessionDescriptionRequest::kAnswer, observer, session_options);
  if (certificate_request_state_ == CERTIFICATE_WAITING) {
    create_session_description_requests_.push(request);
  } else {
    RTC_DCHECK(certificate_request_state_ == CERTIFICATE_SUCCEEDED ||
               certificate_request_state_ == CERTIFICATE_NOT_NEEDED);
    InternalCreateAnswer(request);
  }
}

void WebRtcSessionDescriptionFactory::SetSdesPolicy(
    cricket::SecurePolicy secure_policy) {
  session_desc_factory_.set_secure(secure_policy);
}

cricket::SecurePolicy WebRtcSessionDescriptionFactory::SdesPolicy() const {
  return session_desc_factory_.secure();
}

void WebRtcSessionDescriptionFactory::OnMessage(rtc::Message* msg) {
  switch (msg->message_id) {
    case MSG_CREATE_SESSIONDESCRIPTION_SUCCESS: {
      CreateSessionDescriptionMsg* param =
          static_cast<CreateSessionDescriptionMsg*>(msg->pdata);
      param->observer->OnSuccess(param->description.release());
      delete param;
      break;
    }
    case MSG_CREATE_SESSIONDESCRIPTION_FAILED: {
      CreateSessionDescriptionMsg* param =
          static_cast<CreateSessionDescriptionMsg*>(msg->pdata);
      param->observer->OnFailure(param->error);
      delete param;
      break;
    }
    case MSG_USE_CONSTRUCTOR_CERTIFICATE: {
      rtc::ScopedRefMessageData<rtc::RTCCertificate>* param =
          static_cast<rtc::ScopedRefMessageData<rtc::RTCCertificate>*>(
              msg->pdata);
      LOG(LS_INFO) << "Using certificate supplied to the constructor.";
      SetCertificate(param->data());
      delete param;
      break;
    }
    default:
      RTC_NOTREACHED();
      break;
  }
}

void WebRtcSessionDescriptionFactory::InternalCreateOffer(
    CreateSessionDescriptionRequest request) {
  if (session_->local_description()) {
    for (const cricket::TransportInfo& transport :
         session_->local_description()->description()->transport_infos()) {
      // If the needs-ice-restart flag is set as described by JSEP, we should
      // generate an offer with a new ufrag/password to trigger an ICE restart.
      if (session_->NeedsIceRestart(transport.content_name)) {
        request.options.transport_options[transport.content_name].ice_restart =
            true;
      }
    }
  }

  cricket::SessionDescription* desc(session_desc_factory_.CreateOffer(
      request.options, session_->local_description()
                           ? session_->local_description()->description()
                           : nullptr));
  // RFC 3264
  // When issuing an offer that modifies the session,
  // the "o=" line of the new SDP MUST be identical to that in the
  // previous SDP, except that the version in the origin field MUST
  // increment by one from the previous SDP.

  // Just increase the version number by one each time when a new offer
  // is created regardless if it's identical to the previous one or not.
  // The |session_version_| is a uint64_t, the wrap around should not happen.
  RTC_DCHECK(session_version_ + 1 > session_version_);
  JsepSessionDescription* offer(new JsepSessionDescription(
      JsepSessionDescription::kOffer));
  if (!offer->Initialize(desc, session_id_,
                         rtc::ToString(session_version_++))) {
    delete offer;
    PostCreateSessionDescriptionFailed(request.observer,
                                       "Failed to initialize the offer.");
    return;
  }
  if (session_->local_description()) {
    for (const cricket::ContentInfo& content :
         session_->local_description()->description()->contents()) {
      // Include all local ICE candidates in the SessionDescription unless
      // the remote peer has requested an ICE restart.
      if (!request.options.transport_options[content.name].ice_restart) {
        CopyCandidatesFromSessionDescription(session_->local_description(),
                                             content.name, offer);
      }
    }
  }
  PostCreateSessionDescriptionSucceeded(request.observer, offer);
}

void WebRtcSessionDescriptionFactory::InternalCreateAnswer(
    CreateSessionDescriptionRequest request) {
  if (session_->remote_description()) {
    for (const cricket::ContentInfo& content :
         session_->remote_description()->description()->contents()) {
      // According to http://tools.ietf.org/html/rfc5245#section-9.2.1.1
      // an answer should also contain new ICE ufrag and password if an offer
      // has been received with new ufrag and password.
      request.options.transport_options[content.name].ice_restart =
          session_->IceRestartPending(content.name);
      // We should pass the current SSL role to the transport description
      // factory, if there is already an existing ongoing session.
      rtc::SSLRole ssl_role;
      if (session_->GetSslRole(content.name, &ssl_role)) {
        request.options.transport_options[content.name].prefer_passive_role =
            (rtc::SSL_SERVER == ssl_role);
      }
    }
  }

  cricket::SessionDescription* desc(session_desc_factory_.CreateAnswer(
      session_->remote_description()
          ? session_->remote_description()->description()
          : nullptr,
      request.options, session_->local_description()
                           ? session_->local_description()->description()
                           : nullptr));
  // RFC 3264
  // If the answer is different from the offer in any way (different IP
  // addresses, ports, etc.), the origin line MUST be different in the answer.
  // In that case, the version number in the "o=" line of the answer is
  // unrelated to the version number in the o line of the offer.
  // Get a new version number by increasing the |session_version_answer_|.
  // The |session_version_| is a uint64_t, the wrap around should not happen.
  RTC_DCHECK(session_version_ + 1 > session_version_);
  JsepSessionDescription* answer(new JsepSessionDescription(
      JsepSessionDescription::kAnswer));
  if (!answer->Initialize(desc, session_id_,
                          rtc::ToString(session_version_++))) {
    delete answer;
    PostCreateSessionDescriptionFailed(request.observer,
                                       "Failed to initialize the answer.");
    return;
  }
  if (session_->local_description()) {
    for (const cricket::ContentInfo& content :
         session_->local_description()->description()->contents()) {
      // Include all local ICE candidates in the SessionDescription unless
      // the remote peer has requested an ICE restart.
      if (!request.options.transport_options[content.name].ice_restart) {
        CopyCandidatesFromSessionDescription(session_->local_description(),
                                             content.name, answer);
      }
    }
  }
  PostCreateSessionDescriptionSucceeded(request.observer, answer);
}

void WebRtcSessionDescriptionFactory::FailPendingRequests(
    const std::string& reason) {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  while (!create_session_description_requests_.empty()) {
    const CreateSessionDescriptionRequest& request =
        create_session_description_requests_.front();
    PostCreateSessionDescriptionFailed(request.observer,
        ((request.type == CreateSessionDescriptionRequest::kOffer) ?
            "CreateOffer" : "CreateAnswer") + reason);
    create_session_description_requests_.pop();
  }
}

void WebRtcSessionDescriptionFactory::PostCreateSessionDescriptionFailed(
    CreateSessionDescriptionObserver* observer, const std::string& error) {
  CreateSessionDescriptionMsg* msg = new CreateSessionDescriptionMsg(observer);
  msg->error = error;
  signaling_thread_->Post(RTC_FROM_HERE, this,
                          MSG_CREATE_SESSIONDESCRIPTION_FAILED, msg);
  LOG(LS_ERROR) << "Create SDP failed: " << error;
}

void WebRtcSessionDescriptionFactory::PostCreateSessionDescriptionSucceeded(
    CreateSessionDescriptionObserver* observer,
    SessionDescriptionInterface* description) {
  CreateSessionDescriptionMsg* msg = new CreateSessionDescriptionMsg(observer);
  msg->description.reset(description);
  signaling_thread_->Post(RTC_FROM_HERE, this,
                          MSG_CREATE_SESSIONDESCRIPTION_SUCCESS, msg);
}

void WebRtcSessionDescriptionFactory::OnCertificateRequestFailed() {
  RTC_DCHECK(signaling_thread_->IsCurrent());

  LOG(LS_ERROR) << "Asynchronous certificate generation request failed.";
  certificate_request_state_ = CERTIFICATE_FAILED;

  FailPendingRequests(kFailedDueToIdentityFailed);
}

void WebRtcSessionDescriptionFactory::SetCertificate(
    const rtc::scoped_refptr<rtc::RTCCertificate>& certificate) {
  RTC_DCHECK(certificate);
  LOG(LS_VERBOSE) << "Setting new certificate.";

  certificate_request_state_ = CERTIFICATE_SUCCEEDED;
  SignalCertificateReady(certificate);

  transport_desc_factory_.set_certificate(certificate);
  transport_desc_factory_.set_secure(cricket::SEC_ENABLED);

  while (!create_session_description_requests_.empty()) {
    if (create_session_description_requests_.front().type ==
        CreateSessionDescriptionRequest::kOffer) {
      InternalCreateOffer(create_session_description_requests_.front());
    } else {
      InternalCreateAnswer(create_session_description_requests_.front());
    }
    create_session_description_requests_.pop();
  }
}
}  // namespace webrtc
