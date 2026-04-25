/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/sctp_transport.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <utility>

#include "api/dtls_transport_interface.h"
#include "api/priority.h"
#include "api/rtc_error.h"
#include "api/scoped_refptr.h"
#include "api/sctp_transport_interface.h"
#include "api/sequence_checker.h"
#include "api/transport/data_channel_transport_interface.h"
#include "media/sctp/sctp_transport_internal.h"
#include "p2p/dtls/dtls_transport_internal.h"
#include "pc/dtls_transport.h"
#include "rtc_base/checks.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/logging.h"
#include "rtc_base/ssl_stream_adapter.h"
#include "rtc_base/thread.h"

namespace webrtc {

SctpTransport::SctpTransport(std::unique_ptr<SctpTransportInternal> internal,
                             scoped_refptr<DtlsTransport> dtls_transport)
    : owner_thread_(Thread::Current()),
      info_(SctpTransportState::kConnecting,
            dtls_transport,
            /*max_message_size=*/std::nullopt,
            /*max_channels=*/std::nullopt),
      internal_sctp_transport_(std::move(internal)),
      dtls_transport_(dtls_transport) {
  RTC_DCHECK(internal_sctp_transport_.get());
  RTC_DCHECK(internal_sctp_transport_->dtls_transport());
  RTC_DCHECK(dtls_transport_.get());

  internal_sctp_transport_->dtls_transport()->SubscribeDtlsTransportState(
      this, [this](DtlsTransportInternal* transport, DtlsTransportState state) {
        OnDtlsStateChange(transport, state);
      });

  internal_sctp_transport_->SetOnConnectedCallback(
      [this]() { OnAssociationChangeCommunicationUp(); });
}

SctpTransport::~SctpTransport() {
  // We depend on the network thread to call Clear() before dropping
  // its last reference to this object.
  RTC_DCHECK(owner_thread_->IsCurrent() || !internal_sctp_transport_);
}

SctpTransportInformation SctpTransport::Information() const {
  // TODO(tommi): Update PeerConnection::GetSctpTransport to hand out a proxy
  // to the transport so that we can be sure that methods get called on the
  // expected thread. Chromium currently calls this method from
  // TransceiverStateSurfacer.
  if (!owner_thread_->IsCurrent()) {
    return owner_thread_->BlockingCall([this] { return Information(); });
  }
  RTC_DCHECK_RUN_ON(owner_thread_);
  return info_;
}

void SctpTransport::RegisterObserver(SctpTransportObserverInterface* observer) {
  RTC_DCHECK_RUN_ON(owner_thread_);
  RTC_DCHECK(observer);
  RTC_DCHECK(!observer_);
  observer_ = observer;
}

void SctpTransport::UnregisterObserver() {
  RTC_DCHECK_RUN_ON(owner_thread_);
  observer_ = nullptr;
}

RTCError SctpTransport::OpenChannel(int channel_id, PriorityValue priority) {
  RTC_DCHECK_RUN_ON(owner_thread_);
  RTC_DCHECK(internal_sctp_transport_);
  internal_sctp_transport_->OpenStream(channel_id, priority);
  return RTCError::OK();
}

RTCError SctpTransport::SendData(int channel_id,
                                 const SendDataParams& params,
                                 const CopyOnWriteBuffer& buffer) {
  RTC_DCHECK_RUN_ON(owner_thread_);
  return internal_sctp_transport_->SendData(channel_id, params, buffer);
}

RTCError SctpTransport::CloseChannel(int channel_id) {
  RTC_DCHECK_RUN_ON(owner_thread_);
  RTC_DCHECK(internal_sctp_transport_);
  internal_sctp_transport_->ResetStream(channel_id);
  return RTCError::OK();
}

void SctpTransport::SetDataSink(DataChannelSink* sink) {
  RTC_DCHECK_RUN_ON(owner_thread_);
  RTC_DCHECK(internal_sctp_transport_);
  internal_sctp_transport_->SetDataChannelSink(sink);
}

bool SctpTransport::IsReadyToSend() const {
  RTC_DCHECK_RUN_ON(owner_thread_);
  RTC_DCHECK(internal_sctp_transport_);
  return internal_sctp_transport_->ReadyToSendData();
}

size_t SctpTransport::buffered_amount(int channel_id) const {
  RTC_DCHECK_RUN_ON(owner_thread_);
  RTC_DCHECK(internal_sctp_transport_);
  return internal_sctp_transport_->buffered_amount(channel_id);
}

size_t SctpTransport::buffered_amount_low_threshold(int channel_id) const {
  RTC_DCHECK_RUN_ON(owner_thread_);
  return internal_sctp_transport_->buffered_amount_low_threshold(channel_id);
}

void SctpTransport::SetBufferedAmountLowThreshold(int channel_id,
                                                  size_t bytes) {
  RTC_DCHECK_RUN_ON(owner_thread_);
  internal_sctp_transport_->SetBufferedAmountLowThreshold(channel_id, bytes);
}

std::optional<int> SctpTransport::MaxChannels() {
  RTC_DCHECK_RUN_ON(owner_thread_);
  return info_.MaxChannels();
}

std::optional<SSLRole> SctpTransport::DtlsRole() {
  RTC_DCHECK_RUN_ON(owner_thread_);
  if (!dtls_transport_) {
    return std::nullopt;
  }
  std::optional<DtlsTransportTlsRole> role =
      dtls_transport_->Information().role();
  if (!role.has_value()) {
    return std::nullopt;
  }
  switch (*role) {
    case DtlsTransportTlsRole::kServer:
      return SSL_SERVER;
    case DtlsTransportTlsRole::kClient:
      return SSL_CLIENT;
  }
}

scoped_refptr<DtlsTransportInterface> SctpTransport::dtls_transport() const {
  RTC_DCHECK_RUN_ON(owner_thread_);
  return dtls_transport_;
}

// Internal functions
void SctpTransport::Clear() {
  RTC_DCHECK_RUN_ON(owner_thread_);
  // Note that we delete internal_sctp_transport_, but
  // only drop the reference to dtls_transport_.
  internal_sctp_transport_->dtls_transport()->UnsubscribeDtlsTransportState(
      this);
  dtls_transport_ = nullptr;
  internal_sctp_transport_ = nullptr;
  UpdateInformation(SctpTransportState::kClosed);
}

void SctpTransport::Start(const SctpOptions& options) {
  RTC_DCHECK_RUN_ON(owner_thread_);
  info_ =
      SctpTransportInformation(info_.state(), info_.dtls_transport(),
                               options.max_message_size, info_.MaxChannels());

  if (!internal()->Start(options)) {
    RTC_LOG(LS_ERROR) << "Failed to push down SCTP parameters, closing.";
    UpdateInformation(SctpTransportState::kClosed);
  }
}

void SctpTransport::UpdateInformation(SctpTransportState state) {
  RTC_DCHECK_RUN_ON(owner_thread_);
  bool must_send_update = (state != info_.state());
  // TODO(https://bugs.webrtc.org/10358): Update max channels from internal
  // SCTP transport when available.
  if (internal_sctp_transport_) {
    info_ = SctpTransportInformation(
        state, dtls_transport_, info_.MaxMessageSize(), info_.MaxChannels());
  } else {
    info_ = SctpTransportInformation(
        state, dtls_transport_, info_.MaxMessageSize(), info_.MaxChannels());
  }

  if (observer_ && must_send_update) {
    observer_->OnStateChange(info_);
  }
}

void SctpTransport::OnAssociationChangeCommunicationUp() {
  RTC_DCHECK_RUN_ON(owner_thread_);
  RTC_DCHECK(internal_sctp_transport_);
  if (internal_sctp_transport_->max_outbound_streams() &&
      internal_sctp_transport_->max_inbound_streams()) {
    int max_channels =
        std::min(*(internal_sctp_transport_->max_outbound_streams()),
                 *(internal_sctp_transport_->max_inbound_streams()));
    // Record max channels.
    info_ = SctpTransportInformation(info_.state(), info_.dtls_transport(),
                                     info_.MaxMessageSize(), max_channels);
  }

  UpdateInformation(SctpTransportState::kConnected);
}

void SctpTransport::OnDtlsStateChange(DtlsTransportInternal* transport,
                                      DtlsTransportState state) {
  RTC_DCHECK_RUN_ON(owner_thread_);
  if (state == DtlsTransportState::kClosed ||
      state == DtlsTransportState::kFailed) {
    UpdateInformation(SctpTransportState::kClosed);
    // TODO(http://bugs.webrtc.org/11090): Close all the data channels
  }
}

}  // namespace webrtc
