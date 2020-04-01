/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/data_channel.h"

#include <memory>
#include <string>
#include <utility>

#include "media/sctp/sctp_transport_internal.h"
#include "pc/sctp_utils.h"
#include "rtc_base/checks.h"
#include "rtc_base/location.h"
#include "rtc_base/logging.h"
#include "rtc_base/ref_counted_object.h"
#include "rtc_base/thread.h"

namespace webrtc {

static size_t kMaxQueuedReceivedDataBytes = 16 * 1024 * 1024;
static size_t kMaxQueuedSendDataBytes = 16 * 1024 * 1024;

namespace {

static std::atomic<int> g_unique_id{0};

int GenerateUniqueId() {
  return ++g_unique_id;
}

}  // namespace

InternalDataChannelInit::InternalDataChannelInit(const DataChannelInit& base)
    : DataChannelInit(base), open_handshake_role(kOpener) {
  // If the channel is externally negotiated, do not send the OPEN message.
  if (base.negotiated) {
    open_handshake_role = kNone;
  } else {
    // Datachannel is externally negotiated. Ignore the id value.
    // Specified in createDataChannel, WebRTC spec section 6.1 bullet 13.
    id = -1;
  }
  // Backwards compatibility: If base.maxRetransmits or base.maxRetransmitTime
  // have been set to -1, unset them.
  if (maxRetransmits && *maxRetransmits == -1) {
    RTC_LOG(LS_ERROR)
        << "Accepting maxRetransmits = -1 for backwards compatibility";
    maxRetransmits = absl::nullopt;
  }
  if (maxRetransmitTime && *maxRetransmitTime == -1) {
    RTC_LOG(LS_ERROR)
        << "Accepting maxRetransmitTime = -1 for backwards compatibility";
    maxRetransmitTime = absl::nullopt;
  }
}

bool SctpSidAllocator::AllocateSid(rtc::SSLRole role, int* sid) {
  int potential_sid = (role == rtc::SSL_CLIENT) ? 0 : 1;
  while (!IsSidAvailable(potential_sid)) {
    potential_sid += 2;
    if (potential_sid > static_cast<int>(cricket::kMaxSctpSid)) {
      return false;
    }
  }

  *sid = potential_sid;
  used_sids_.insert(potential_sid);
  return true;
}

bool SctpSidAllocator::ReserveSid(int sid) {
  if (!IsSidAvailable(sid)) {
    return false;
  }
  used_sids_.insert(sid);
  return true;
}

void SctpSidAllocator::ReleaseSid(int sid) {
  auto it = used_sids_.find(sid);
  if (it != used_sids_.end()) {
    used_sids_.erase(it);
  }
}

bool SctpSidAllocator::IsSidAvailable(int sid) const {
  if (sid < static_cast<int>(cricket::kMinSctpSid) ||
      sid > static_cast<int>(cricket::kMaxSctpSid)) {
    return false;
  }
  return used_sids_.find(sid) == used_sids_.end();
}

bool DataChannel::PacketQueue::Empty() const {
  return packets_.empty();
}

std::unique_ptr<DataBuffer> DataChannel::PacketQueue::PopFront() {
  RTC_DCHECK(!packets_.empty());
  byte_count_ -= packets_.front()->size();
  std::unique_ptr<DataBuffer> packet = std::move(packets_.front());
  packets_.pop_front();
  return packet;
}

void DataChannel::PacketQueue::PushFront(std::unique_ptr<DataBuffer> packet) {
  byte_count_ += packet->size();
  packets_.push_front(std::move(packet));
}

void DataChannel::PacketQueue::PushBack(std::unique_ptr<DataBuffer> packet) {
  byte_count_ += packet->size();
  packets_.push_back(std::move(packet));
}

void DataChannel::PacketQueue::Clear() {
  packets_.clear();
  byte_count_ = 0;
}

void DataChannel::PacketQueue::Swap(PacketQueue* other) {
  size_t other_byte_count = other->byte_count_;
  other->byte_count_ = byte_count_;
  byte_count_ = other_byte_count;

  other->packets_.swap(packets_);
}

rtc::scoped_refptr<DataChannel> DataChannel::Create(
    DataChannelProviderInterface* provider,
    cricket::DataChannelType dct,
    const std::string& label,
    const InternalDataChannelInit& config) {
  rtc::scoped_refptr<DataChannel> channel(
      new rtc::RefCountedObject<DataChannel>(provider, dct, label));
  if (!channel->Init(config)) {
    return NULL;
  }
  return channel;
}

bool DataChannel::IsSctpLike(cricket::DataChannelType type) {
  return type == cricket::DCT_SCTP || type == cricket::DCT_MEDIA_TRANSPORT ||
         type == cricket::DCT_DATA_CHANNEL_TRANSPORT ||
         type == cricket::DCT_DATA_CHANNEL_TRANSPORT_SCTP;
}

DataChannel::DataChannel(DataChannelProviderInterface* provider,
                         cricket::DataChannelType dct,
                         const std::string& label)
    : internal_id_(GenerateUniqueId()),
      label_(label),
      observer_(nullptr),
      state_(kConnecting),
      messages_sent_(0),
      bytes_sent_(0),
      messages_received_(0),
      bytes_received_(0),
      buffered_amount_(0),
      data_channel_type_(dct),
      provider_(provider),
      handshake_state_(kHandshakeInit),
      connected_to_provider_(false),
      send_ssrc_set_(false),
      receive_ssrc_set_(false),
      writable_(false),
      send_ssrc_(0),
      receive_ssrc_(0) {}

bool DataChannel::Init(const InternalDataChannelInit& config) {
  if (data_channel_type_ == cricket::DCT_RTP) {
    if (config.reliable || config.id != -1 || config.maxRetransmits ||
        config.maxRetransmitTime) {
      RTC_LOG(LS_ERROR) << "Failed to initialize the RTP data channel due to "
                           "invalid DataChannelInit.";
      return false;
    }
    handshake_state_ = kHandshakeReady;
  } else if (IsSctpLike(data_channel_type_)) {
    if (config.id < -1 ||
        (config.maxRetransmits && *config.maxRetransmits < 0) ||
        (config.maxRetransmitTime && *config.maxRetransmitTime < 0)) {
      RTC_LOG(LS_ERROR) << "Failed to initialize the SCTP data channel due to "
                           "invalid DataChannelInit.";
      return false;
    }
    if (config.maxRetransmits && config.maxRetransmitTime) {
      RTC_LOG(LS_ERROR)
          << "maxRetransmits and maxRetransmitTime should not be both set.";
      return false;
    }
    config_ = config;

    switch (config_.open_handshake_role) {
      case webrtc::InternalDataChannelInit::kNone:  // pre-negotiated
        handshake_state_ = kHandshakeReady;
        break;
      case webrtc::InternalDataChannelInit::kOpener:
        handshake_state_ = kHandshakeShouldSendOpen;
        break;
      case webrtc::InternalDataChannelInit::kAcker:
        handshake_state_ = kHandshakeShouldSendAck;
        break;
    }

    // Try to connect to the transport in case the transport channel already
    // exists.
    OnTransportChannelCreated();

    // Checks if the transport is ready to send because the initial channel
    // ready signal may have been sent before the DataChannel creation.
    // This has to be done async because the upper layer objects (e.g.
    // Chrome glue and WebKit) are not wired up properly until after this
    // function returns.
    if (provider_->ReadyToSendData()) {
      invoker_.AsyncInvoke<void>(RTC_FROM_HERE, rtc::Thread::Current(),
                                 [this] { OnChannelReady(true); });
    }
  }

  return true;
}

DataChannel::~DataChannel() {}

void DataChannel::RegisterObserver(DataChannelObserver* observer) {
  observer_ = observer;
  DeliverQueuedReceivedData();
}

void DataChannel::UnregisterObserver() {
  observer_ = NULL;
}

bool DataChannel::reliable() const {
  if (data_channel_type_ == cricket::DCT_RTP) {
    return false;
  } else {
    return !config_.maxRetransmits && !config_.maxRetransmitTime;
  }
}

uint64_t DataChannel::buffered_amount() const {
  return buffered_amount_;
}

void DataChannel::Close() {
  if (state_ == kClosed)
    return;
  send_ssrc_ = 0;
  send_ssrc_set_ = false;
  SetState(kClosing);
  // Will send queued data before beginning the underlying closing procedure.
  UpdateState();
}

RTCError DataChannel::error() const {
  return error_;
}

bool DataChannel::Send(const DataBuffer& buffer) {
  buffered_amount_ += buffer.size();
  if (state_ != kOpen) {
    return false;
  }

  // TODO(jiayl): the spec is unclear about if the remote side should get the
  // onmessage event. We need to figure out the expected behavior and change the
  // code accordingly.
  if (buffer.size() == 0) {
    return true;
  }

  // If the queue is non-empty, we're waiting for SignalReadyToSend,
  // so just add to the end of the queue and keep waiting.
  if (!queued_send_data_.Empty()) {
    // Only SCTP DataChannel queues the outgoing data when the transport is
    // blocked.
    RTC_DCHECK(IsSctpLike(data_channel_type_));
    if (!QueueSendDataMessage(buffer)) {
      RTC_LOG(LS_ERROR) << "Closing the DataChannel due to a failure to queue "
                           "additional data.";
      // https://w3c.github.io/webrtc-pc/#dom-rtcdatachannel-send step 5
      // Note that the spec doesn't explicitly say to close in this situation.
      CloseAbruptlyWithError(RTCError(RTCErrorType::RESOURCE_EXHAUSTED,
                                      "Unable to queue data for sending"));
    }
    return true;
  }

  bool success = SendDataMessage(buffer, true);
  if (data_channel_type_ == cricket::DCT_RTP) {
    return success;
  }

  // Always return true for SCTP DataChannel per the spec.
  return true;
}

void DataChannel::SetReceiveSsrc(uint32_t receive_ssrc) {
  RTC_DCHECK(data_channel_type_ == cricket::DCT_RTP);

  if (receive_ssrc_set_) {
    return;
  }
  receive_ssrc_ = receive_ssrc;
  receive_ssrc_set_ = true;
  UpdateState();
}

void DataChannel::SetSctpSid(int sid) {
  RTC_DCHECK_LT(config_.id, 0);
  RTC_DCHECK_GE(sid, 0);
  RTC_DCHECK(IsSctpLike(data_channel_type_));
  if (config_.id == sid) {
    return;
  }

  config_.id = sid;
  provider_->AddSctpDataStream(sid);
}

void DataChannel::OnClosingProcedureStartedRemotely(int sid) {
  if (IsSctpLike(data_channel_type_) && sid == config_.id &&
      state_ != kClosing && state_ != kClosed) {
    // Don't bother sending queued data since the side that initiated the
    // closure wouldn't receive it anyway. See crbug.com/559394 for a lengthy
    // discussion about this.
    queued_send_data_.Clear();
    queued_control_data_.Clear();
    // Just need to change state to kClosing, SctpTransport will handle the
    // rest of the closing procedure and OnClosingProcedureComplete will be
    // called later.
    started_closing_procedure_ = true;
    SetState(kClosing);
  }
}

void DataChannel::OnClosingProcedureComplete(int sid) {
  if (IsSctpLike(data_channel_type_) && sid == config_.id) {
    // If the closing procedure is complete, we should have finished sending
    // all pending data and transitioned to kClosing already.
    RTC_DCHECK_EQ(state_, kClosing);
    RTC_DCHECK(queued_send_data_.Empty());
    DisconnectFromProvider();
    SetState(kClosed);
  }
}

void DataChannel::OnTransportChannelCreated() {
  RTC_DCHECK(IsSctpLike(data_channel_type_));
  if (!connected_to_provider_) {
    connected_to_provider_ = provider_->ConnectDataChannel(this);
  }
  // The sid may have been unassigned when provider_->ConnectDataChannel was
  // done. So always add the streams even if connected_to_provider_ is true.
  if (config_.id >= 0) {
    provider_->AddSctpDataStream(config_.id);
  }
}

void DataChannel::OnTransportChannelClosed() {
  // The SctpTransport is unusable (for example, because the SCTP m= section
  // was rejected, or because the DTLS transport closed), so we need to close
  // abruptly.
  // Note: this needs to differentiate between normal close and error close.
  // https://w3c.github.io/webrtc-pc/#announcing-a-data-channel-as-closed
  CloseAbruptlyWithError(
      RTCError(RTCErrorType::NETWORK_ERROR, "Transport channel closed"));
}

// The remote peer request that this channel shall be closed.
void DataChannel::RemotePeerRequestClose() {
  RTC_DCHECK(data_channel_type_ == cricket::DCT_RTP);
  // Close with error code explicitly set to OK.
  CloseAbruptlyWithError(RTCError());
}

void DataChannel::SetSendSsrc(uint32_t send_ssrc) {
  RTC_DCHECK(data_channel_type_ == cricket::DCT_RTP);
  if (send_ssrc_set_) {
    return;
  }
  send_ssrc_ = send_ssrc;
  send_ssrc_set_ = true;
  UpdateState();
}

void DataChannel::OnDataReceived(const cricket::ReceiveDataParams& params,
                                 const rtc::CopyOnWriteBuffer& payload) {
  if (data_channel_type_ == cricket::DCT_RTP && params.ssrc != receive_ssrc_) {
    return;
  }
  if (IsSctpLike(data_channel_type_) && params.sid != config_.id) {
    return;
  }

  if (params.type == cricket::DMT_CONTROL) {
    RTC_DCHECK(IsSctpLike(data_channel_type_));
    if (handshake_state_ != kHandshakeWaitingForAck) {
      // Ignore it if we are not expecting an ACK message.
      RTC_LOG(LS_WARNING)
          << "DataChannel received unexpected CONTROL message, sid = "
          << params.sid;
      return;
    }
    if (ParseDataChannelOpenAckMessage(payload)) {
      // We can send unordered as soon as we receive the ACK message.
      handshake_state_ = kHandshakeReady;
      RTC_LOG(LS_INFO) << "DataChannel received OPEN_ACK message, sid = "
                       << params.sid;
    } else {
      RTC_LOG(LS_WARNING)
          << "DataChannel failed to parse OPEN_ACK message, sid = "
          << params.sid;
    }
    return;
  }

  RTC_DCHECK(params.type == cricket::DMT_BINARY ||
             params.type == cricket::DMT_TEXT);

  RTC_LOG(LS_VERBOSE) << "DataChannel received DATA message, sid = "
                      << params.sid;
  // We can send unordered as soon as we receive any DATA message since the
  // remote side must have received the OPEN (and old clients do not send
  // OPEN_ACK).
  if (handshake_state_ == kHandshakeWaitingForAck) {
    handshake_state_ = kHandshakeReady;
  }

  bool binary = (params.type == cricket::DMT_BINARY);
  auto buffer = std::make_unique<DataBuffer>(payload, binary);
  if (state_ == kOpen && observer_) {
    ++messages_received_;
    bytes_received_ += buffer->size();
    observer_->OnMessage(*buffer.get());
  } else {
    if (queued_received_data_.byte_count() + payload.size() >
        kMaxQueuedReceivedDataBytes) {
      RTC_LOG(LS_ERROR) << "Queued received data exceeds the max buffer size.";

      queued_received_data_.Clear();
      if (data_channel_type_ != cricket::DCT_RTP) {
        CloseAbruptlyWithError(
            RTCError(RTCErrorType::RESOURCE_EXHAUSTED,
                     "Queued received data exceeds the max buffer size."));
      }

      return;
    }
    queued_received_data_.PushBack(std::move(buffer));
  }
}

void DataChannel::OnChannelReady(bool writable) {
  writable_ = writable;
  if (!writable) {
    return;
  }

  SendQueuedControlMessages();
  SendQueuedDataMessages();
  UpdateState();
}

void DataChannel::CloseAbruptlyWithError(RTCError error) {
  if (state_ == kClosed) {
    return;
  }

  if (connected_to_provider_) {
    DisconnectFromProvider();
  }

  // Closing abruptly means any queued data gets thrown away.
  queued_send_data_.Clear();
  buffered_amount_ = 0;
  queued_control_data_.Clear();

  // Still go to "kClosing" before "kClosed", since observers may be expecting
  // that.
  SetState(kClosing);
  error_ = std::move(error);
  SetState(kClosed);
}

void DataChannel::CloseAbruptlyWithDataChannelFailure(
    const std::string& message) {
  RTCError error(RTCErrorType::OPERATION_ERROR_WITH_DATA, message);
  error.set_error_detail(RTCErrorDetailType::DATA_CHANNEL_FAILURE);
  CloseAbruptlyWithError(std::move(error));
}

void DataChannel::UpdateState() {
  // UpdateState determines what to do from a few state variables.  Include
  // all conditions required for each state transition here for
  // clarity. OnChannelReady(true) will send any queued data and then invoke
  // UpdateState().
  switch (state_) {
    case kConnecting: {
      if (send_ssrc_set_ == receive_ssrc_set_) {
        if (data_channel_type_ == cricket::DCT_RTP && !connected_to_provider_) {
          connected_to_provider_ = provider_->ConnectDataChannel(this);
        }
        if (connected_to_provider_) {
          if (handshake_state_ == kHandshakeShouldSendOpen) {
            rtc::CopyOnWriteBuffer payload;
            WriteDataChannelOpenMessage(label_, config_, &payload);
            SendControlMessage(payload);
          } else if (handshake_state_ == kHandshakeShouldSendAck) {
            rtc::CopyOnWriteBuffer payload;
            WriteDataChannelOpenAckMessage(&payload);
            SendControlMessage(payload);
          }
          if (writable_ && (handshake_state_ == kHandshakeReady ||
                            handshake_state_ == kHandshakeWaitingForAck)) {
            SetState(kOpen);
            // If we have received buffers before the channel got writable.
            // Deliver them now.
            DeliverQueuedReceivedData();
          }
        }
      }
      break;
    }
    case kOpen: {
      break;
    }
    case kClosing: {
      // Wait for all queued data to be sent before beginning the closing
      // procedure.
      if (queued_send_data_.Empty() && queued_control_data_.Empty()) {
        if (data_channel_type_ == cricket::DCT_RTP) {
          // For RTP data channels, we can go to "closed" after we finish
          // sending data and the send/recv SSRCs are unset.
          if (connected_to_provider_) {
            DisconnectFromProvider();
          }
          if (!send_ssrc_set_ && !receive_ssrc_set_) {
            SetState(kClosed);
          }
        } else {
          // For SCTP data channels, we need to wait for the closing procedure
          // to complete; after calling RemoveSctpDataStream,
          // OnClosingProcedureComplete will end up called asynchronously
          // afterwards.
          if (connected_to_provider_ && !started_closing_procedure_ &&
              config_.id >= 0) {
            started_closing_procedure_ = true;
            provider_->RemoveSctpDataStream(config_.id);
          }
        }
      }
      break;
    }
    case kClosed:
      break;
  }
}

void DataChannel::SetState(DataState state) {
  if (state_ == state) {
    return;
  }

  state_ = state;
  if (observer_) {
    observer_->OnStateChange();
  }
  if (state_ == kOpen) {
    SignalOpened(this);
  } else if (state_ == kClosed) {
    SignalClosed(this);
  }
}

void DataChannel::DisconnectFromProvider() {
  if (!connected_to_provider_)
    return;

  provider_->DisconnectDataChannel(this);
  connected_to_provider_ = false;
}

void DataChannel::DeliverQueuedReceivedData() {
  if (!observer_) {
    return;
  }

  while (!queued_received_data_.Empty()) {
    std::unique_ptr<DataBuffer> buffer = queued_received_data_.PopFront();
    ++messages_received_;
    bytes_received_ += buffer->size();
    observer_->OnMessage(*buffer);
  }
}

void DataChannel::SendQueuedDataMessages() {
  if (queued_send_data_.Empty()) {
    return;
  }

  RTC_DCHECK(state_ == kOpen || state_ == kClosing);

  while (!queued_send_data_.Empty()) {
    std::unique_ptr<DataBuffer> buffer = queued_send_data_.PopFront();
    if (!SendDataMessage(*buffer, false)) {
      // Return the message to the front of the queue if sending is aborted.
      queued_send_data_.PushFront(std::move(buffer));
      break;
    }
  }
}

bool DataChannel::SendDataMessage(const DataBuffer& buffer,
                                  bool queue_if_blocked) {
  cricket::SendDataParams send_params;

  if (IsSctpLike(data_channel_type_)) {
    send_params.ordered = config_.ordered;
    // Send as ordered if it is still going through OPEN/ACK signaling.
    if (handshake_state_ != kHandshakeReady && !config_.ordered) {
      send_params.ordered = true;
      RTC_LOG(LS_VERBOSE)
          << "Sending data as ordered for unordered DataChannel "
             "because the OPEN_ACK message has not been received.";
    }

    send_params.max_rtx_count =
        config_.maxRetransmits ? *config_.maxRetransmits : -1;
    send_params.max_rtx_ms =
        config_.maxRetransmitTime ? *config_.maxRetransmitTime : -1;
    send_params.sid = config_.id;
  } else {
    send_params.ssrc = send_ssrc_;
  }
  send_params.type = buffer.binary ? cricket::DMT_BINARY : cricket::DMT_TEXT;

  cricket::SendDataResult send_result = cricket::SDR_SUCCESS;
  bool success = provider_->SendData(send_params, buffer.data, &send_result);

  if (success) {
    ++messages_sent_;
    bytes_sent_ += buffer.size();

    RTC_DCHECK(buffered_amount_ >= buffer.size());
    buffered_amount_ -= buffer.size();
    if (observer_ && buffer.size() > 0) {
      observer_->OnBufferedAmountChange(buffer.size());
    }
    return true;
  }

  if (!IsSctpLike(data_channel_type_)) {
    return false;
  }

  if (send_result == cricket::SDR_BLOCK) {
    if (!queue_if_blocked || QueueSendDataMessage(buffer)) {
      return false;
    }
  }
  // Close the channel if the error is not SDR_BLOCK, or if queuing the
  // message failed.
  RTC_LOG(LS_ERROR) << "Closing the DataChannel due to a failure to send data, "
                       "send_result = "
                    << send_result;
  CloseAbruptlyWithError(
      RTCError(RTCErrorType::NETWORK_ERROR, "Failure to send data"));

  return false;
}

bool DataChannel::QueueSendDataMessage(const DataBuffer& buffer) {
  size_t start_buffered_amount = queued_send_data_.byte_count();
  if (start_buffered_amount + buffer.size() > kMaxQueuedSendDataBytes) {
    RTC_LOG(LS_ERROR) << "Can't buffer any more data for the data channel.";
    return false;
  }
  queued_send_data_.PushBack(std::make_unique<DataBuffer>(buffer));
  return true;
}

void DataChannel::SendQueuedControlMessages() {
  PacketQueue control_packets;
  control_packets.Swap(&queued_control_data_);

  while (!control_packets.Empty()) {
    std::unique_ptr<DataBuffer> buf = control_packets.PopFront();
    SendControlMessage(buf->data);
  }
}

void DataChannel::QueueControlMessage(const rtc::CopyOnWriteBuffer& buffer) {
  queued_control_data_.PushBack(std::make_unique<DataBuffer>(buffer, true));
}

bool DataChannel::SendControlMessage(const rtc::CopyOnWriteBuffer& buffer) {
  bool is_open_message = handshake_state_ == kHandshakeShouldSendOpen;

  RTC_DCHECK(IsSctpLike(data_channel_type_));
  RTC_DCHECK(writable_);
  RTC_DCHECK_GE(config_.id, 0);
  RTC_DCHECK(!is_open_message || !config_.negotiated);

  cricket::SendDataParams send_params;
  send_params.sid = config_.id;
  // Send data as ordered before we receive any message from the remote peer to
  // make sure the remote peer will not receive any data before it receives the
  // OPEN message.
  send_params.ordered = config_.ordered || is_open_message;
  send_params.type = cricket::DMT_CONTROL;

  cricket::SendDataResult send_result = cricket::SDR_SUCCESS;
  bool retval = provider_->SendData(send_params, buffer, &send_result);
  if (retval) {
    RTC_LOG(LS_VERBOSE) << "Sent CONTROL message on channel " << config_.id;

    if (handshake_state_ == kHandshakeShouldSendAck) {
      handshake_state_ = kHandshakeReady;
    } else if (handshake_state_ == kHandshakeShouldSendOpen) {
      handshake_state_ = kHandshakeWaitingForAck;
    }
  } else if (send_result == cricket::SDR_BLOCK) {
    QueueControlMessage(buffer);
  } else {
    RTC_LOG(LS_ERROR) << "Closing the DataChannel due to a failure to send"
                         " the CONTROL message, send_result = "
                      << send_result;
    CloseAbruptlyWithError(RTCError(RTCErrorType::NETWORK_ERROR,
                                    "Failed to send a CONTROL message"));
  }
  return retval;
}

// static
void DataChannel::ResetInternalIdAllocatorForTesting(int new_value) {
  g_unique_id = new_value;
}

}  // namespace webrtc
