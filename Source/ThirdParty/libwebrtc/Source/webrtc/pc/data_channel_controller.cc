/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/data_channel_controller.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "api/data_channel_event_observer_interface.h"
#include "api/data_channel_interface.h"
#include "api/priority.h"
#include "api/rtc_error.h"
#include "api/scoped_refptr.h"
#include "api/sctp_transport_interface.h"
#include "api/sequence_checker.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "api/transport/data_channel_transport_interface.h"
#include "pc/data_channel_utils.h"
#include "pc/peer_connection_internal.h"
#include "pc/sctp_data_channel.h"
#include "pc/sctp_utils.h"
#include "rtc_base/checks.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/logging.h"
#include "rtc_base/ssl_stream_adapter.h"
#include "rtc_base/thread.h"
#include "rtc_base/time_utils.h"

namespace webrtc {

using Message = DataChannelEventObserverInterface::Message;
using Direction = DataChannelEventObserverInterface::Message::Direction;

DataChannelController::~DataChannelController() {
  RTC_DCHECK(sctp_data_channels_n_.empty())
      << "Missing call to TeardownDataChannelTransport_n?";
  RTC_DCHECK(!signaling_safety_.flag()->alive())
      << "Missing call to PrepareForShutdown?";
}

bool DataChannelController::HasDataChannels() const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  return channel_usage_ == DataChannelUsage::kInUse;
}

bool DataChannelController::HasUsedDataChannels() const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  return channel_usage_ != DataChannelUsage::kNeverUsed;
}

void DataChannelController::SetEventObserver(
    std::unique_ptr<DataChannelEventObserverInterface> observer) {
  RTC_DCHECK_RUN_ON(network_thread());
  event_observer_ = std::move(observer);
}

RTCError DataChannelController::SendData(StreamId sid,
                                         const SendDataParams& params,
                                         const CopyOnWriteBuffer& payload) {
  RTC_DCHECK_RUN_ON(network_thread());
  if (!data_channel_transport_) {
    RTC_LOG(LS_ERROR) << "SendData called before transport is ready";
    return RTCError(RTCErrorType::INVALID_STATE);
  }
  RTCError result =
      data_channel_transport_->SendData(sid.stream_id_int(), params, payload);

  if (event_observer_ && result.ok()) {
    if (std::optional<Message> message =
            BuildObserverMessage(sid, params.type, payload, Direction::kSend)) {
      event_observer_->OnMessage(*message);
    }
  }

  return result;
}

RTCError DataChannelController::AddSctpDataStream(StreamId sid,
                                                  PriorityValue priority) {
  RTC_DCHECK_RUN_ON(network_thread());
  if (data_channel_transport_) {
    return data_channel_transport_->OpenChannel(sid.stream_id_int(), priority);
  }
  return RTCError::OK();
}

void DataChannelController::RemoveSctpDataStream(StreamId sid) {
  RTC_DCHECK_RUN_ON(network_thread());
  if (data_channel_transport_) {
    data_channel_transport_->CloseChannel(sid.stream_id_int());
  }
}

void DataChannelController::OnChannelStateChanged(
    SctpDataChannel* channel,
    DataChannelInterface::DataState state) {
  RTC_DCHECK_RUN_ON(network_thread());
  // Stash away the internal id here in case `OnSctpDataChannelClosed` ends up
  // releasing the last reference to the channel.
  const int channel_id = channel->internal_id();

  if (state == DataChannelInterface::DataState::kClosed)
    OnSctpDataChannelClosed(channel);

  DataChannelUsage channel_usage = sctp_data_channels_n_.empty()
                                       ? DataChannelUsage::kHaveBeenUsed
                                       : DataChannelUsage::kInUse;
  signaling_thread()->PostTask(SafeTask(
      signaling_safety_.flag(), [this, channel_id, state, channel_usage] {
        RTC_DCHECK_RUN_ON(signaling_thread());
        channel_usage_ = channel_usage;
        pc_->OnSctpDataChannelStateChanged(channel_id, state);
      }));
}

size_t DataChannelController::buffered_amount(StreamId sid) const {
  RTC_DCHECK_RUN_ON(network_thread());
  if (!data_channel_transport_) {
    return 0;
  }
  return data_channel_transport_->buffered_amount(sid.stream_id_int());
}

size_t DataChannelController::buffered_amount_low_threshold(
    StreamId sid) const {
  RTC_DCHECK_RUN_ON(network_thread());
  if (!data_channel_transport_) {
    return 0;
  }
  return data_channel_transport_->buffered_amount_low_threshold(
      sid.stream_id_int());
}

void DataChannelController::SetBufferedAmountLowThreshold(StreamId sid,
                                                          size_t bytes) {
  RTC_DCHECK_RUN_ON(network_thread());
  if (!data_channel_transport_) {
    return;
  }
  data_channel_transport_->SetBufferedAmountLowThreshold(sid.stream_id_int(),
                                                         bytes);
}

void DataChannelController::OnTransportConnected() {
  RTC_DCHECK_RUN_ON(network_thread());
  RTC_DCHECK(data_channel_transport_);
  RTC_DCHECK(data_channel_transport_->MaxChannels().has_value());
  sid_allocator_.SetMaxSid(*data_channel_transport_->MaxChannels() - 1);
  RTC_DCHECK(data_channel_transport_->DtlsRole().has_value());
  AllocateSctpSids(*data_channel_transport_->DtlsRole());
}

void DataChannelController::OnDataReceived(int channel_id,
                                           DataMessageType type,
                                           const CopyOnWriteBuffer& buffer) {
  RTC_DCHECK_RUN_ON(network_thread());

  if (HandleOpenMessage_n(channel_id, type, buffer))
    return;

  auto it = absl::c_find_if(sctp_data_channels_n_, [&](const auto& c) {
    return c->sid_n().has_value() && c->sid_n()->stream_id_int() == channel_id;
  });

  if (it != sctp_data_channels_n_.end()) {
    (*it)->OnDataReceived(type, buffer);

    if (event_observer_) {
      if (std::optional<Message> message = BuildObserverMessage(
              StreamId(channel_id), type, buffer, Direction::kReceive)) {
        event_observer_->OnMessage(*message);
      }
    }
  }
}

void DataChannelController::OnChannelClosing(int channel_id) {
  RTC_DCHECK_RUN_ON(network_thread());
  auto it = absl::c_find_if(sctp_data_channels_n_, [&](const auto& c) {
    return c->sid_n().has_value() && c->sid_n()->stream_id_int() == channel_id;
  });

  if (it != sctp_data_channels_n_.end())
    (*it)->OnClosingProcedureStartedRemotely();
}

void DataChannelController::OnChannelClosed(int channel_id) {
  RTC_DCHECK_RUN_ON(network_thread());
  StreamId sid(channel_id);
  sid_allocator_.ReleaseSid(sid);
  auto it = absl::c_find_if(sctp_data_channels_n_,
                            [&](const auto& c) { return c->sid_n() == sid; });

  if (it != sctp_data_channels_n_.end()) {
    scoped_refptr<SctpDataChannel> channel = std::move(*it);
    sctp_data_channels_n_.erase(it);
    channel->OnClosingProcedureComplete();
  }
}

void DataChannelController::OnReadyToSend() {
  RTC_DCHECK_RUN_ON(network_thread());
  auto copy = sctp_data_channels_n_;
  for (const auto& channel : copy) {
    // All channels are supposed to either have an ID allocated in
    // OnConnected, or be deleted at this point.
    RTC_DCHECK(channel->sid_n().has_value());
    channel->OnTransportReady();
  }
}

void DataChannelController::OnTransportClosed(RTCError error) {
  RTC_DCHECK_RUN_ON(network_thread());

  // This loop will close all data channels and trigger a callback to
  // `OnSctpDataChannelClosed`. We'll empty `sctp_data_channels_n_`, first
  // and `OnSctpDataChannelClosed` will become a noop but we'll release the
  // StreamId here.
  std::vector<scoped_refptr<SctpDataChannel>> temp_sctp_dcs;
  temp_sctp_dcs.swap(sctp_data_channels_n_);
  for (const auto& channel : temp_sctp_dcs) {
    channel->OnTransportChannelClosed(error);
    if (channel->sid_n().has_value()) {
      sid_allocator_.ReleaseSid(*channel->sid_n());
    }
  }
}

void DataChannelController::OnBufferedAmountLow(int channel_id) {
  RTC_DCHECK_RUN_ON(network_thread());
  auto it = absl::c_find_if(sctp_data_channels_n_, [&](const auto& c) {
    return c->sid_n().has_value() && c->sid_n()->stream_id_int() == channel_id;
  });

  if (it != sctp_data_channels_n_.end())
    (*it)->OnBufferedAmountLow();
}

void DataChannelController::SetupDataChannelTransport_n(
    DataChannelTransportInterface* transport) {
  RTC_DCHECK_RUN_ON(network_thread());
  RTC_DCHECK(transport);
  set_data_channel_transport(transport);
}

void DataChannelController::PrepareForShutdown() {
  RTC_DCHECK_RUN_ON(signaling_thread());
  signaling_safety_.reset(PendingTaskSafetyFlag::CreateDetachedInactive());
  if (channel_usage_ != DataChannelUsage::kNeverUsed)
    channel_usage_ = DataChannelUsage::kHaveBeenUsed;
}

void DataChannelController::TeardownDataChannelTransport_n(RTCError error) {
  RTC_DCHECK_RUN_ON(network_thread());
  OnTransportClosed(error);
  set_data_channel_transport(nullptr);
  RTC_DCHECK(sctp_data_channels_n_.empty());
  weak_factory_.InvalidateWeakPtrs();
}

void DataChannelController::OnTransportChanged(
    DataChannelTransportInterface* new_data_channel_transport) {
  RTC_DCHECK_RUN_ON(network_thread());
  if (data_channel_transport_ &&
      data_channel_transport_ != new_data_channel_transport) {
    // Changed which data channel transport is used for `sctp_mid_` (eg. now
    // it's bundled).
    set_data_channel_transport(new_data_channel_transport);
  }
}

std::vector<DataChannelStats> DataChannelController::GetDataChannelStats()
    const {
  RTC_DCHECK_RUN_ON(network_thread());
  std::vector<DataChannelStats> stats;
  stats.reserve(sctp_data_channels_n_.size());
  for (const auto& channel : sctp_data_channels_n_)
    stats.push_back(channel->GetStats());
  return stats;
}

bool DataChannelController::HandleOpenMessage_n(
    int channel_id,
    DataMessageType type,
    const CopyOnWriteBuffer& buffer) {
  if (type != DataMessageType::kControl || !IsOpenMessage(buffer))
    return false;

  // Received OPEN message; parse and signal that a new data channel should
  // be created.
  std::string label;
  InternalDataChannelInit config;
  config.id = channel_id;
  if (!ParseDataChannelOpenMessage(buffer, &label, &config)) {
    RTC_LOG(LS_WARNING) << "Failed to parse the OPEN message for sid "
                        << channel_id;
  } else {
    config.open_handshake_role = InternalDataChannelInit::kAcker;
    auto channel_or_error = CreateDataChannel(label, config);
    if (channel_or_error.ok()) {
      signaling_thread()->PostTask(SafeTask(
          signaling_safety_.flag(),
          [this, channel = channel_or_error.MoveValue(),
           ready_to_send = data_channel_transport_->IsReadyToSend()] {
            RTC_DCHECK_RUN_ON(signaling_thread());
            OnDataChannelOpenMessage(std::move(channel), ready_to_send);
          }));
    } else {
      RTC_LOG(LS_ERROR) << "Failed to create DataChannel from the OPEN message."
                        << ToString(channel_or_error.error().type());
    }
  }
  return true;
}

void DataChannelController::OnDataChannelOpenMessage(
    scoped_refptr<SctpDataChannel> channel,
    bool ready_to_send) {
  channel_usage_ = DataChannelUsage::kInUse;
  auto proxy = SctpDataChannel::CreateProxy(channel, signaling_safety_.flag());

  pc_->RunWithObserver([&](auto observer) { observer->OnDataChannel(proxy); });
  pc_->NoteDataAddedEvent();

  if (ready_to_send) {
    network_thread()->PostTask([channel = std::move(channel)] {
      if (channel->state() != DataChannelInterface::DataState::kClosed)
        channel->OnTransportReady();
    });
  }
}

// RTC_RUN_ON(network_thread())
RTCError DataChannelController::ReserveOrAllocateSid(
    std::optional<StreamId>& sid,
    std::optional<SSLRole> fallback_ssl_role) {
  if (sid.has_value()) {
    return sid_allocator_.ReserveSid(*sid)
               ? RTCError::OK()
               : RTCError(RTCErrorType::INVALID_RANGE, "StreamId reserved.");
  }

  // Attempt to allocate an ID based on the negotiated role.
  std::optional<SSLRole> role = pc_->GetSctpSslRole_n();
  if (!role)
    role = fallback_ssl_role;
  if (role) {
    sid = sid_allocator_.AllocateSid(*role);
    if (!sid.has_value())
      return RTCError(RTCErrorType::RESOURCE_EXHAUSTED);
  }
  // When we get here, we may still not have an ID, but that's a supported case
  // whereby an id will be assigned later.
  RTC_DCHECK(sid.has_value() || !role);
  return RTCError::OK();
}

// RTC_RUN_ON(network_thread())
RTCErrorOr<scoped_refptr<SctpDataChannel>>
DataChannelController::CreateDataChannel(absl::string_view label,
                                         InternalDataChannelInit& config) {
  std::optional<StreamId> sid = std::nullopt;
  if (config.id != -1) {
    if (config.id < 0 || config.id > kMaxSctpSid) {
      return RTCError(RTCErrorType::INVALID_RANGE, "StreamId out of range.");
    }
    sid = StreamId(config.id);
  }

  RTCError err = ReserveOrAllocateSid(sid, config.fallback_ssl_role);
  if (!err.ok())
    return err;

  // In case `sid` has changed. Update `config` accordingly.
  if (sid.has_value()) {
    config.id = sid->stream_id_int();
  }

  scoped_refptr<SctpDataChannel> channel = SctpDataChannel::Create(
      weak_factory_.GetWeakPtr(), label, data_channel_transport_ != nullptr,
      config, signaling_thread(), network_thread());
  RTC_DCHECK(channel);

  // If we have an id already, notify the transport.
  if (sid.has_value()) {
    RTCError error = AddSctpDataStream(
        *sid, config.priority.value_or(PriorityValue(Priority::kLow)));
    if (!error.ok()) {
      return error;
    }
  }
  sctp_data_channels_n_.push_back(channel);
  return channel;
}

RTCErrorOr<scoped_refptr<DataChannelInterface>>
DataChannelController::InternalCreateDataChannelWithProxy(
    absl::string_view label,
    const InternalDataChannelInit& config) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  RTC_DCHECK(!pc_->IsClosed());
  if (!config.IsValid()) {
    return LOG_ERROR(RTCError::InvalidParameter() << "Invalid DataChannelInit");
  }

  bool ready_to_send = false;
  InternalDataChannelInit new_config = config;
  auto ret = network_thread()->BlockingCall(
      [&]() -> RTCErrorOr<scoped_refptr<SctpDataChannel>> {
        RTC_DCHECK_RUN_ON(network_thread());
        auto channel = CreateDataChannel(label, new_config);
        if (!channel.ok())
          return channel;
        ready_to_send =
            data_channel_transport_ && data_channel_transport_->IsReadyToSend();
        if (ready_to_send) {
          // If the transport is ready to send because the initial channel
          // ready signal may have been sent before the DataChannel creation.
          // This has to be done async because the upper layer objects (e.g.
          // Chrome glue and WebKit) are not wired up properly until after
          // `InternalCreateDataChannelWithProxy` returns.
          network_thread()->PostTask([channel = channel.value()] {
            if (channel->state() != DataChannelInterface::DataState::kClosed)
              channel->OnTransportReady();
          });
        }

        return channel;
      });

  if (!ret.ok())
    return ret.MoveError();

  channel_usage_ = DataChannelUsage::kInUse;
  return SctpDataChannel::CreateProxy(ret.MoveValue(),
                                      signaling_safety_.flag());
}

void DataChannelController::AllocateSctpSids(SSLRole role) {
  RTC_DCHECK_RUN_ON(network_thread());

  const bool ready_to_send =
      data_channel_transport_ && data_channel_transport_->IsReadyToSend();

  std::vector<SctpDataChannel*> channels_to_start;
  std::vector<scoped_refptr<SctpDataChannel>> channels_to_close;
  for (auto it = sctp_data_channels_n_.begin();
       it != sctp_data_channels_n_.end();) {
    if (!(*it)->sid_n().has_value()) {
      std::optional<StreamId> sid = sid_allocator_.AllocateSid(role);
      if (sid.has_value()) {
        (*it)->SetSctpSid_n(*sid);
        AddSctpDataStream(*sid, (*it)->priority());
        channels_to_start.push_back((*it).get());
      } else {
        channels_to_close.push_back(std::move(*it));
        it = sctp_data_channels_n_.erase(it);
        continue;
      }
    }
    ++it;
  }
  // Since OnTransportReady can cause sending, and sending may fail and cause
  // channel to close, do this outside the loop.
  if (ready_to_send) {
    for (auto* channel : channels_to_start) {
      RTC_LOG(LS_INFO) << "AllocateSctpSids: Id assigned, ready to send.";
      channel->OnTransportReady();
    }
  }

  // Since closing modifies the list of channels, we have to do the actual
  // closing outside the loop.
  for (const auto& channel : channels_to_close) {
    channel->CloseAbruptlyWithDataChannelFailure("Failed to allocate SCTP SID");
  }
}

void DataChannelController::OnSctpDataChannelClosed(SctpDataChannel* channel) {
  RTC_DCHECK_RUN_ON(network_thread());
  // After the closing procedure is done, it's safe to use this ID for
  // another data channel.
  if (channel->sid_n().has_value()) {
    sid_allocator_.ReleaseSid(*channel->sid_n());
  }
  auto it = absl::c_find_if(sctp_data_channels_n_,
                            [&](const auto& c) { return c.get() == channel; });
  if (it != sctp_data_channels_n_.end()) {
    sctp_data_channels_n_.erase(it);
  }
}

void DataChannelController::set_data_channel_transport(
    DataChannelTransportInterface* transport) {
  RTC_DCHECK_RUN_ON(network_thread());

  if (data_channel_transport_)
    data_channel_transport_->SetDataSink(nullptr);

  data_channel_transport_ = transport;

  if (data_channel_transport_) {
    // There's a new data channel transport.  This needs to be signaled to the
    // `sctp_data_channels_n_` so that they can reopen and reconnect.  This is
    // necessary when bundling is applied.
    NotifyDataChannelsOfTransportCreated();
    data_channel_transport_->SetDataSink(this);
  }
}

std::optional<Message> DataChannelController::BuildObserverMessage(
    StreamId sid,
    DataMessageType type,
    ArrayView<const uint8_t> payload,
    Message::Direction direction) const {
  RTC_DCHECK_RUN_ON(network_thread());

  if (type != DataMessageType::kText && type != DataMessageType::kBinary) {
    return std::nullopt;
  }

  auto it = absl::c_find_if(sctp_data_channels_n_, [sid](const auto& channel) {
    return channel->sid_n() == sid;
  });

  if (it == sctp_data_channels_n_.end()) {
    return std::nullopt;
  }

  Message message;
  Message::DataType data_type = type == DataMessageType::kBinary
                                    ? Message::DataType::kBinary
                                    : Message::DataType::kString;
  message.set_data_type(data_type);
  message.set_unix_timestamp_ms(TimeUTCMillis());
  message.set_datachannel_id(sid.stream_id_int());
  message.set_label((*it)->label());
  message.set_direction(direction);
  message.set_data(payload);

  return message;
}

void DataChannelController::NotifyDataChannelsOfTransportCreated() {
  RTC_DCHECK_RUN_ON(network_thread());
  RTC_DCHECK(data_channel_transport_);

  for (const auto& channel : sctp_data_channels_n_) {
    if (channel->sid_n().has_value())
      AddSctpDataStream(*channel->sid_n(), channel->priority());
    channel->OnTransportChannelCreated();
  }
}

Thread* DataChannelController::network_thread() const {
  return pc_->network_thread();
}

Thread* DataChannelController::signaling_thread() const {
  return pc_->signaling_thread();
}

}  // namespace webrtc
