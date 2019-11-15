/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/dtls_transport.h"

#include <utility>

#include "pc/ice_transport.h"

namespace webrtc {

namespace {

DtlsTransportState TranslateState(cricket::DtlsTransportState internal_state) {
  switch (internal_state) {
    case cricket::DTLS_TRANSPORT_NEW:
      return DtlsTransportState::kNew;
      break;
    case cricket::DTLS_TRANSPORT_CONNECTING:
      return DtlsTransportState::kConnecting;
      break;
    case cricket::DTLS_TRANSPORT_CONNECTED:
      return DtlsTransportState::kConnected;
      break;
    case cricket::DTLS_TRANSPORT_CLOSED:
      return DtlsTransportState::kClosed;
      break;
    case cricket::DTLS_TRANSPORT_FAILED:
      return DtlsTransportState::kFailed;
      break;
  }
}

}  // namespace

// Implementation of DtlsTransportInterface
DtlsTransport::DtlsTransport(
    std::unique_ptr<cricket::DtlsTransportInternal> internal)
    : owner_thread_(rtc::Thread::Current()),
      info_(DtlsTransportState::kNew),
      internal_dtls_transport_(std::move(internal)),
      ice_transport_(new rtc::RefCountedObject<IceTransportWithPointer>(
          internal_dtls_transport_->ice_transport())) {
  RTC_DCHECK(internal_dtls_transport_.get());
  internal_dtls_transport_->SignalDtlsState.connect(
      this, &DtlsTransport::OnInternalDtlsState);
  UpdateInformation();
}

DtlsTransport::~DtlsTransport() {
  // We depend on the signaling thread to call Clear() before dropping
  // its last reference to this object.
  RTC_DCHECK(owner_thread_->IsCurrent() || !internal_dtls_transport_);
}

DtlsTransportInformation DtlsTransport::Information() {
  rtc::CritScope scope(&lock_);
  return info_;
}

void DtlsTransport::RegisterObserver(DtlsTransportObserverInterface* observer) {
  RTC_DCHECK_RUN_ON(owner_thread_);
  RTC_DCHECK(observer);
  observer_ = observer;
}

void DtlsTransport::UnregisterObserver() {
  RTC_DCHECK_RUN_ON(owner_thread_);
  observer_ = nullptr;
}

rtc::scoped_refptr<IceTransportInterface> DtlsTransport::ice_transport() {
  return ice_transport_;
}

// Internal functions
void DtlsTransport::Clear() {
  RTC_DCHECK_RUN_ON(owner_thread_);
  RTC_DCHECK(internal());
  bool must_send_event =
      (internal()->dtls_state() != cricket::DTLS_TRANSPORT_CLOSED);
  // The destructor of cricket::DtlsTransportInternal calls back
  // into DtlsTransport, so we can't hold the lock while releasing.
  std::unique_ptr<cricket::DtlsTransportInternal> transport_to_release;
  {
    rtc::CritScope scope(&lock_);
    transport_to_release = std::move(internal_dtls_transport_);
    ice_transport_->Clear();
  }
  UpdateInformation();
  if (observer_ && must_send_event) {
    observer_->OnStateChange(Information());
  }
}

void DtlsTransport::OnInternalDtlsState(
    cricket::DtlsTransportInternal* transport,
    cricket::DtlsTransportState state) {
  RTC_DCHECK_RUN_ON(owner_thread_);
  RTC_DCHECK(transport == internal());
  RTC_DCHECK(state == internal()->dtls_state());
  UpdateInformation();
  if (observer_) {
    observer_->OnStateChange(Information());
  }
}

void DtlsTransport::UpdateInformation() {
  RTC_DCHECK_RUN_ON(owner_thread_);
  rtc::CritScope scope(&lock_);
  if (internal_dtls_transport_) {
    if (internal_dtls_transport_->dtls_state() ==
        cricket::DTLS_TRANSPORT_CONNECTED) {
      int ssl_cipher_suite;
      if (internal_dtls_transport_->GetSslCipherSuite(&ssl_cipher_suite)) {
        info_ = DtlsTransportInformation(
            TranslateState(internal_dtls_transport_->dtls_state()),
            ssl_cipher_suite,
            internal_dtls_transport_->GetRemoteSSLCertChain());
      } else {
        info_ = DtlsTransportInformation(
            TranslateState(internal_dtls_transport_->dtls_state()),
            absl::nullopt, internal_dtls_transport_->GetRemoteSSLCertChain());
      }
    } else {
      info_ = DtlsTransportInformation(
          TranslateState(internal_dtls_transport_->dtls_state()));
    }
  } else {
    info_ = DtlsTransportInformation(DtlsTransportState::kClosed);
  }
}

}  // namespace webrtc
