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

#include <optional>

#include "api/dtls_transport_interface.h"
#include "api/ice_transport_interface.h"
#include "api/make_ref_counted.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "p2p/dtls/dtls_transport_internal.h"
#include "pc/ice_transport.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/ssl_stream_adapter.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread.h"

namespace webrtc {

// Implementation of DtlsTransportInterface
DtlsTransport::DtlsTransport(DtlsTransportInternal* internal,
                             DtlsTransportObserverInterface* observer)
    : observer_(observer),
      owner_thread_(Thread::Current()),
      info_(DtlsTransportState::kNew),
      ice_transport_(make_ref_counted<IceTransportWithPointer>(
          internal->ice_transport())) {
  RTC_DCHECK(internal);
  UpdateInformation(internal);
}

DtlsTransport::~DtlsTransport() = default;

void DtlsTransport::RegisterObserver(DtlsTransportObserverInterface* observer) {
  RTC_DCHECK_RUN_ON(owner_thread_);
  RTC_DCHECK(observer);
  observer_ = observer;
}

void DtlsTransport::UnregisterObserver() {
  RTC_DCHECK_RUN_ON(owner_thread_);
  observer_ = nullptr;
}

DtlsTransportInformation DtlsTransport::Information() {
  MutexLock lock(&lock_);
  return info_;
}

scoped_refptr<IceTransportInterface> DtlsTransport::ice_transport() {
  return ice_transport_;
}

// Internal functions
void DtlsTransport::Clear(DtlsTransportInternal* internal) {
  RTC_DCHECK_RUN_ON(owner_thread_);
  bool must_send_event =
      (internal->dtls_state() != DtlsTransportState::kClosed);
  ice_transport_->Clear();
  UpdateInformation(nullptr);
  if (observer_ && must_send_event) {
    observer_->OnStateChange(Information());
  }
}

void DtlsTransport::OnInternalDtlsState(DtlsTransportInternal* transport) {
  RTC_DCHECK_RUN_ON(owner_thread_);
  UpdateInformation(transport);
  if (observer_) {
    observer_->OnStateChange(Information());
  }
}

void DtlsTransport::UpdateInformation(DtlsTransportInternal* internal) {
  RTC_DCHECK_RUN_ON(owner_thread_);
  if (internal) {
    if (internal->dtls_state() == DtlsTransportState::kConnected) {
      bool success = true;
      SSLRole internal_role;
      std::optional<DtlsTransportTlsRole> role;
      int ssl_cipher_suite;
      int tls_version;
      int srtp_cipher;
      success &= internal->GetDtlsRole(&internal_role);
      if (success) {
        switch (internal_role) {
          case SSL_CLIENT:
            role = DtlsTransportTlsRole::kClient;
            break;
          case SSL_SERVER:
            role = DtlsTransportTlsRole::kServer;
            break;
        }
      }
      success &= internal->GetSslVersionBytes(&tls_version);
      success &= internal->GetSslCipherSuite(&ssl_cipher_suite);
      success &= internal->GetSrtpCryptoSuite(&srtp_cipher);
      if (success) {
        set_info(DtlsTransportInformation(
            internal->dtls_state(), role, tls_version, ssl_cipher_suite,
            srtp_cipher, internal->GetRemoteSSLCertChain(),
            internal->GetSslGroupId()));
      } else {
        RTC_LOG(LS_ERROR) << "DtlsTransport in connected state has incomplete "
                             "TLS information";
        set_info(DtlsTransportInformation(
            internal->dtls_state(), role, std::nullopt, std::nullopt,
            std::nullopt, internal->GetRemoteSSLCertChain(),
            /* ssl_group_id= */ std::nullopt));
      }
    } else {
      set_info(DtlsTransportInformation(internal->dtls_state()));
    }
  } else {
    set_info(DtlsTransportInformation(DtlsTransportState::kClosed));
  }
}

}  // namespace webrtc
