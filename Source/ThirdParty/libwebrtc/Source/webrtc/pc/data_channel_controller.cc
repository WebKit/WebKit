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

#include <utility>

#include "pc/peer_connection.h"
#include "pc/sctp_utils.h"

namespace webrtc {

bool DataChannelController::HasDataChannels() const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  return !rtp_data_channels_.empty() || !sctp_data_channels_.empty();
}

bool DataChannelController::SendData(const cricket::SendDataParams& params,
                                     const rtc::CopyOnWriteBuffer& payload,
                                     cricket::SendDataResult* result) {
  // RTC_DCHECK_RUN_ON(signaling_thread());
  if (data_channel_transport()) {
    SendDataParams send_params;
    send_params.type = ToWebrtcDataMessageType(params.type);
    send_params.ordered = params.ordered;
    if (params.max_rtx_count >= 0) {
      send_params.max_rtx_count = params.max_rtx_count;
    } else if (params.max_rtx_ms >= 0) {
      send_params.max_rtx_ms = params.max_rtx_ms;
    }

    RTCError error = network_thread()->Invoke<RTCError>(
        RTC_FROM_HERE, [this, params, send_params, payload] {
          return data_channel_transport()->SendData(params.sid, send_params,
                                                    payload);
        });

    if (error.ok()) {
      *result = cricket::SendDataResult::SDR_SUCCESS;
      return true;
    } else if (error.type() == RTCErrorType::RESOURCE_EXHAUSTED) {
      // SCTP transport uses RESOURCE_EXHAUSTED when it's blocked.
      // TODO(mellem):  Stop using RTCError here and get rid of the mapping.
      *result = cricket::SendDataResult::SDR_BLOCK;
      return false;
    }
    *result = cricket::SendDataResult::SDR_ERROR;
    return false;
  } else if (rtp_data_channel()) {
    return rtp_data_channel()->SendData(params, payload, result);
  }
  RTC_LOG(LS_ERROR) << "SendData called before transport is ready";
  return false;
}

bool DataChannelController::ConnectDataChannel(
    DataChannel* webrtc_data_channel) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  if (!rtp_data_channel() && !data_channel_transport()) {
    // Don't log an error here, because DataChannels are expected to call
    // ConnectDataChannel in this state. It's the only way to initially tell
    // whether or not the underlying transport is ready.
    return false;
  }
  if (data_channel_transport()) {
    SignalDataChannelTransportWritable_s.connect(webrtc_data_channel,
                                                 &DataChannel::OnChannelReady);
    SignalDataChannelTransportReceivedData_s.connect(
        webrtc_data_channel, &DataChannel::OnDataReceived);
    SignalDataChannelTransportChannelClosing_s.connect(
        webrtc_data_channel, &DataChannel::OnClosingProcedureStartedRemotely);
    SignalDataChannelTransportChannelClosed_s.connect(
        webrtc_data_channel, &DataChannel::OnClosingProcedureComplete);
  }
  if (rtp_data_channel()) {
    rtp_data_channel()->SignalReadyToSendData.connect(
        webrtc_data_channel, &DataChannel::OnChannelReady);
    rtp_data_channel()->SignalDataReceived.connect(
        webrtc_data_channel, &DataChannel::OnDataReceived);
  }
  return true;
}

void DataChannelController::DisconnectDataChannel(
    DataChannel* webrtc_data_channel) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  if (!rtp_data_channel() && !data_channel_transport()) {
    RTC_LOG(LS_ERROR)
        << "DisconnectDataChannel called when rtp_data_channel_ and "
           "sctp_transport_ are NULL.";
    return;
  }
  if (data_channel_transport()) {
    SignalDataChannelTransportWritable_s.disconnect(webrtc_data_channel);
    SignalDataChannelTransportReceivedData_s.disconnect(webrtc_data_channel);
    SignalDataChannelTransportChannelClosing_s.disconnect(webrtc_data_channel);
    SignalDataChannelTransportChannelClosed_s.disconnect(webrtc_data_channel);
  }
  if (rtp_data_channel()) {
    rtp_data_channel()->SignalReadyToSendData.disconnect(webrtc_data_channel);
    rtp_data_channel()->SignalDataReceived.disconnect(webrtc_data_channel);
  }
}

void DataChannelController::AddSctpDataStream(int sid) {
  if (data_channel_transport()) {
    network_thread()->Invoke<void>(RTC_FROM_HERE, [this, sid] {
      if (data_channel_transport()) {
        data_channel_transport()->OpenChannel(sid);
      }
    });
  }
}

void DataChannelController::RemoveSctpDataStream(int sid) {
  if (data_channel_transport()) {
    network_thread()->Invoke<void>(RTC_FROM_HERE, [this, sid] {
      if (data_channel_transport()) {
        data_channel_transport()->CloseChannel(sid);
      }
    });
  }
}

bool DataChannelController::ReadyToSendData() const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  return (rtp_data_channel() && rtp_data_channel()->ready_to_send_data()) ||
         (data_channel_transport() && data_channel_transport_ready_to_send_);
}

void DataChannelController::OnDataReceived(
    int channel_id,
    DataMessageType type,
    const rtc::CopyOnWriteBuffer& buffer) {
  RTC_DCHECK_RUN_ON(network_thread());
  cricket::ReceiveDataParams params;
  params.sid = channel_id;
  params.type = ToCricketDataMessageType(type);
  data_channel_transport_invoker_->AsyncInvoke<void>(
      RTC_FROM_HERE, signaling_thread(), [this, params, buffer] {
        RTC_DCHECK_RUN_ON(signaling_thread());
        if (!HandleOpenMessage_s(params, buffer)) {
          SignalDataChannelTransportReceivedData_s(params, buffer);
        }
      });
}

void DataChannelController::OnChannelClosing(int channel_id) {
  RTC_DCHECK_RUN_ON(network_thread());
  data_channel_transport_invoker_->AsyncInvoke<void>(
      RTC_FROM_HERE, signaling_thread(), [this, channel_id] {
        RTC_DCHECK_RUN_ON(signaling_thread());
        SignalDataChannelTransportChannelClosing_s(channel_id);
      });
}

void DataChannelController::OnChannelClosed(int channel_id) {
  RTC_DCHECK_RUN_ON(network_thread());
  data_channel_transport_invoker_->AsyncInvoke<void>(
      RTC_FROM_HERE, signaling_thread(), [this, channel_id] {
        RTC_DCHECK_RUN_ON(signaling_thread());
        SignalDataChannelTransportChannelClosed_s(channel_id);
      });
}

void DataChannelController::OnReadyToSend() {
  RTC_DCHECK_RUN_ON(network_thread());
  data_channel_transport_invoker_->AsyncInvoke<void>(
      RTC_FROM_HERE, signaling_thread(), [this] {
        RTC_DCHECK_RUN_ON(signaling_thread());
        data_channel_transport_ready_to_send_ = true;
        SignalDataChannelTransportWritable_s(
            data_channel_transport_ready_to_send_);
      });
}

void DataChannelController::OnTransportClosed() {
  RTC_DCHECK_RUN_ON(network_thread());
  data_channel_transport_invoker_->AsyncInvoke<void>(
      RTC_FROM_HERE, signaling_thread(), [this] {
        RTC_DCHECK_RUN_ON(signaling_thread());
        OnTransportChannelClosed();
      });
}

void DataChannelController::SetupDataChannelTransport_n() {
  RTC_DCHECK_RUN_ON(network_thread());
  data_channel_transport_invoker_ = std::make_unique<rtc::AsyncInvoker>();
}

void DataChannelController::TeardownDataChannelTransport_n() {
  RTC_DCHECK_RUN_ON(network_thread());
  data_channel_transport_invoker_ = nullptr;
  if (data_channel_transport()) {
    data_channel_transport()->SetDataSink(nullptr);
  }
  set_data_channel_transport(nullptr);
}

void DataChannelController::OnTransportChanged(
    DataChannelTransportInterface* new_data_channel_transport) {
  RTC_DCHECK_RUN_ON(network_thread());
  if (data_channel_transport() &&
      data_channel_transport() != new_data_channel_transport) {
    // Changed which data channel transport is used for |sctp_mid_| (eg. now
    // it's bundled).
    data_channel_transport()->SetDataSink(nullptr);
    set_data_channel_transport(new_data_channel_transport);
    if (new_data_channel_transport) {
      new_data_channel_transport->SetDataSink(this);

      // There's a new data channel transport.  This needs to be signaled to the
      // |sctp_data_channels_| so that they can reopen and reconnect.  This is
      // necessary when bundling is applied.
      data_channel_transport_invoker_->AsyncInvoke<void>(
          RTC_FROM_HERE, signaling_thread(), [this] {
            RTC_DCHECK_RUN_ON(signaling_thread());
            for (auto channel : sctp_data_channels_) {
              channel->OnTransportChannelCreated();
            }
          });
    }
  }
}

bool DataChannelController::HandleOpenMessage_s(
    const cricket::ReceiveDataParams& params,
    const rtc::CopyOnWriteBuffer& buffer) {
  if (params.type == cricket::DMT_CONTROL && IsOpenMessage(buffer)) {
    // Received OPEN message; parse and signal that a new data channel should
    // be created.
    std::string label;
    InternalDataChannelInit config;
    config.id = params.ssrc;
    if (!ParseDataChannelOpenMessage(buffer, &label, &config)) {
      RTC_LOG(LS_WARNING) << "Failed to parse the OPEN message for ssrc "
                          << params.ssrc;
      return true;
    }
    config.open_handshake_role = InternalDataChannelInit::kAcker;
    OnDataChannelOpenMessage(label, config);
    return true;
  }
  return false;
}

void DataChannelController::OnDataChannelOpenMessage(
    const std::string& label,
    const InternalDataChannelInit& config) {
  rtc::scoped_refptr<DataChannel> channel(
      InternalCreateDataChannel(label, &config));
  if (!channel.get()) {
    RTC_LOG(LS_ERROR) << "Failed to create DataChannel from the OPEN message.";
    return;
  }

  rtc::scoped_refptr<DataChannelInterface> proxy_channel =
      DataChannelProxy::Create(signaling_thread(), channel);
  pc_->Observer()->OnDataChannel(std::move(proxy_channel));
  pc_->NoteDataAddedEvent();
}

rtc::scoped_refptr<DataChannel>
DataChannelController::InternalCreateDataChannel(
    const std::string& label,
    const InternalDataChannelInit* config) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  if (pc_->IsClosed()) {
    return nullptr;
  }
  if (data_channel_type_ == cricket::DCT_NONE) {
    RTC_LOG(LS_ERROR)
        << "InternalCreateDataChannel: Data is not supported in this call.";
    return nullptr;
  }
  InternalDataChannelInit new_config =
      config ? (*config) : InternalDataChannelInit();
  if (DataChannel::IsSctpLike(data_channel_type_)) {
    if (new_config.id < 0) {
      rtc::SSLRole role;
      if ((pc_->GetSctpSslRole(&role)) &&
          !sid_allocator_.AllocateSid(role, &new_config.id)) {
        RTC_LOG(LS_ERROR)
            << "No id can be allocated for the SCTP data channel.";
        return nullptr;
      }
    } else if (!sid_allocator_.ReserveSid(new_config.id)) {
      RTC_LOG(LS_ERROR) << "Failed to create a SCTP data channel "
                           "because the id is already in use or out of range.";
      return nullptr;
    }
  }

  rtc::scoped_refptr<DataChannel> channel(
      DataChannel::Create(this, data_channel_type(), label, new_config));
  if (!channel) {
    sid_allocator_.ReleaseSid(new_config.id);
    return nullptr;
  }

  if (channel->data_channel_type() == cricket::DCT_RTP) {
    if (rtp_data_channels_.find(channel->label()) != rtp_data_channels_.end()) {
      RTC_LOG(LS_ERROR) << "DataChannel with label " << channel->label()
                        << " already exists.";
      return nullptr;
    }
    rtp_data_channels_[channel->label()] = channel;
  } else {
    RTC_DCHECK(DataChannel::IsSctpLike(data_channel_type_));
    sctp_data_channels_.push_back(channel);
    channel->SignalClosed.connect(pc_,
                                  &PeerConnection::OnSctpDataChannelClosed);
  }
  SignalDataChannelCreated_(channel.get());
  return channel;
}

void DataChannelController::AllocateSctpSids(rtc::SSLRole role) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  std::vector<rtc::scoped_refptr<DataChannel>> channels_to_close;
  for (const auto& channel : sctp_data_channels_) {
    if (channel->id() < 0) {
      int sid;
      if (!sid_allocator_.AllocateSid(role, &sid)) {
        RTC_LOG(LS_ERROR) << "Failed to allocate SCTP sid, closing channel.";
        channels_to_close.push_back(channel);
        continue;
      }
      channel->SetSctpSid(sid);
    }
  }
  // Since closing modifies the list of channels, we have to do the actual
  // closing outside the loop.
  for (const auto& channel : channels_to_close) {
    channel->CloseAbruptlyWithDataChannelFailure("Failed to allocate SCTP SID");
  }
}

void DataChannelController::OnSctpDataChannelClosed(DataChannel* channel) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  for (auto it = sctp_data_channels_.begin(); it != sctp_data_channels_.end();
       ++it) {
    if (it->get() == channel) {
      if (channel->id() >= 0) {
        // After the closing procedure is done, it's safe to use this ID for
        // another data channel.
        sid_allocator_.ReleaseSid(channel->id());
      }
      // Since this method is triggered by a signal from the DataChannel,
      // we can't free it directly here; we need to free it asynchronously.
      sctp_data_channels_to_free_.push_back(*it);
      sctp_data_channels_.erase(it);
      signaling_thread()->PostTask(
          RTC_FROM_HERE, [self = weak_factory_.GetWeakPtr()] {
            if (self) {
              RTC_DCHECK_RUN_ON(self->signaling_thread());
              self->sctp_data_channels_to_free_.clear();
            }
          });
      return;
    }
  }
}

void DataChannelController::OnTransportChannelClosed() {
  RTC_DCHECK_RUN_ON(signaling_thread());
  // Use a temporary copy of the RTP/SCTP DataChannel list because the
  // DataChannel may callback to us and try to modify the list.
  std::map<std::string, rtc::scoped_refptr<DataChannel>> temp_rtp_dcs;
  temp_rtp_dcs.swap(rtp_data_channels_);
  for (const auto& kv : temp_rtp_dcs) {
    kv.second->OnTransportChannelClosed();
  }

  std::vector<rtc::scoped_refptr<DataChannel>> temp_sctp_dcs;
  temp_sctp_dcs.swap(sctp_data_channels_);
  for (const auto& channel : temp_sctp_dcs) {
    channel->OnTransportChannelClosed();
  }
}

DataChannel* DataChannelController::FindDataChannelBySid(int sid) const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  for (const auto& channel : sctp_data_channels_) {
    if (channel->id() == sid) {
      return channel;
    }
  }
  return nullptr;
}

void DataChannelController::UpdateLocalRtpDataChannels(
    const cricket::StreamParamsVec& streams) {
  std::vector<std::string> existing_channels;

  RTC_DCHECK_RUN_ON(signaling_thread());
  // Find new and active data channels.
  for (const cricket::StreamParams& params : streams) {
    // |it->sync_label| is actually the data channel label. The reason is that
    // we use the same naming of data channels as we do for
    // MediaStreams and Tracks.
    // For MediaStreams, the sync_label is the MediaStream label and the
    // track label is the same as |streamid|.
    const std::string& channel_label = params.first_stream_id();
    auto data_channel_it = rtp_data_channels()->find(channel_label);
    if (data_channel_it == rtp_data_channels()->end()) {
      RTC_LOG(LS_ERROR) << "channel label not found";
      continue;
    }
    // Set the SSRC the data channel should use for sending.
    data_channel_it->second->SetSendSsrc(params.first_ssrc());
    existing_channels.push_back(data_channel_it->first);
  }

  UpdateClosingRtpDataChannels(existing_channels, true);
}

void DataChannelController::UpdateRemoteRtpDataChannels(
    const cricket::StreamParamsVec& streams) {
  std::vector<std::string> existing_channels;

  RTC_DCHECK_RUN_ON(signaling_thread());
  // Find new and active data channels.
  for (const cricket::StreamParams& params : streams) {
    // The data channel label is either the mslabel or the SSRC if the mslabel
    // does not exist. Ex a=ssrc:444330170 mslabel:test1.
    std::string label = params.first_stream_id().empty()
                            ? rtc::ToString(params.first_ssrc())
                            : params.first_stream_id();
    auto data_channel_it = rtp_data_channels()->find(label);
    if (data_channel_it == rtp_data_channels()->end()) {
      // This is a new data channel.
      CreateRemoteRtpDataChannel(label, params.first_ssrc());
    } else {
      data_channel_it->second->SetReceiveSsrc(params.first_ssrc());
    }
    existing_channels.push_back(label);
  }

  UpdateClosingRtpDataChannels(existing_channels, false);
}

void DataChannelController::UpdateClosingRtpDataChannels(
    const std::vector<std::string>& active_channels,
    bool is_local_update) {
  auto it = rtp_data_channels_.begin();
  while (it != rtp_data_channels_.end()) {
    DataChannel* data_channel = it->second;
    if (absl::c_linear_search(active_channels, data_channel->label())) {
      ++it;
      continue;
    }

    if (is_local_update) {
      data_channel->SetSendSsrc(0);
    } else {
      data_channel->RemotePeerRequestClose();
    }

    if (data_channel->state() == DataChannel::kClosed) {
      rtp_data_channels_.erase(it);
      it = rtp_data_channels_.begin();
    } else {
      ++it;
    }
  }
}

void DataChannelController::CreateRemoteRtpDataChannel(const std::string& label,
                                                       uint32_t remote_ssrc) {
  rtc::scoped_refptr<DataChannel> channel(
      InternalCreateDataChannel(label, nullptr));
  if (!channel.get()) {
    RTC_LOG(LS_WARNING) << "Remote peer requested a DataChannel but"
                           "CreateDataChannel failed.";
    return;
  }
  channel->SetReceiveSsrc(remote_ssrc);
  rtc::scoped_refptr<DataChannelInterface> proxy_channel =
      DataChannelProxy::Create(signaling_thread(), channel);
  pc_->Observer()->OnDataChannel(std::move(proxy_channel));
}

rtc::Thread* DataChannelController::network_thread() const {
  return pc_->network_thread();
}
rtc::Thread* DataChannelController::signaling_thread() const {
  return pc_->signaling_thread();
}

}  // namespace webrtc
