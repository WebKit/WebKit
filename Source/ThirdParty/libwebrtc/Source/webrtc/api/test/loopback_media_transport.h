/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_LOOPBACK_MEDIA_TRANSPORT_H_
#define API_TEST_LOOPBACK_MEDIA_TRANSPORT_H_

#include <memory>
#include <utility>

#include "api/media_transport_interface.h"
#include "rtc_base/asyncinvoker.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/thread.h"
#include "rtc_base/thread_checker.h"

namespace webrtc {

// Wrapper used to hand out unique_ptrs to loopback media transports without
// ownership changes.
class WrapperMediaTransport : public MediaTransportInterface {
 public:
  explicit WrapperMediaTransport(MediaTransportInterface* wrapped)
      : wrapped_(wrapped) {}

  RTCError SendAudioFrame(uint64_t channel_id,
                          MediaTransportEncodedAudioFrame frame) override {
    return wrapped_->SendAudioFrame(channel_id, std::move(frame));
  }

  RTCError SendVideoFrame(
      uint64_t channel_id,
      const MediaTransportEncodedVideoFrame& frame) override {
    return wrapped_->SendVideoFrame(channel_id, frame);
  }

  RTCError RequestKeyFrame(uint64_t channel_id) override {
    return wrapped_->RequestKeyFrame(channel_id);
  }

  void SetReceiveAudioSink(MediaTransportAudioSinkInterface* sink) override {
    wrapped_->SetReceiveAudioSink(sink);
  }

  void SetReceiveVideoSink(MediaTransportVideoSinkInterface* sink) override {
    wrapped_->SetReceiveVideoSink(sink);
  }

  void SetMediaTransportStateCallback(
      MediaTransportStateCallback* callback) override {
    wrapped_->SetMediaTransportStateCallback(callback);
  }

  RTCError SendData(int channel_id,
                    const SendDataParams& params,
                    const rtc::CopyOnWriteBuffer& buffer) override {
    return wrapped_->SendData(channel_id, params, buffer);
  }

  RTCError CloseChannel(int channel_id) override {
    return wrapped_->CloseChannel(channel_id);
  }

  void SetDataSink(DataChannelSink* sink) override {
    wrapped_->SetDataSink(sink);
  }

 private:
  MediaTransportInterface* wrapped_;
};

class WrapperMediaTransportFactory : public MediaTransportFactory {
 public:
  explicit WrapperMediaTransportFactory(MediaTransportInterface* wrapped)
      : wrapped_(wrapped) {}

  RTCErrorOr<std::unique_ptr<MediaTransportInterface>> CreateMediaTransport(
      rtc::PacketTransportInternal* packet_transport,
      rtc::Thread* network_thread,
      const MediaTransportSettings& settings) override {
    return {absl::make_unique<WrapperMediaTransport>(wrapped_)};
  }

 private:
  MediaTransportInterface* wrapped_;
};

// Contains two MediaTransportsInterfaces that are connected to each other.
// Currently supports audio only.
class MediaTransportPair {
 public:
  struct Stats {
    int sent_audio_frames = 0;
    int received_audio_frames = 0;
  };

  explicit MediaTransportPair(rtc::Thread* thread)
      : first_(thread, &second_), second_(thread, &first_) {}

  // Ownership stays with MediaTransportPair
  MediaTransportInterface* first() { return &first_; }
  MediaTransportInterface* second() { return &second_; }

  std::unique_ptr<MediaTransportFactory> first_factory() {
    return absl::make_unique<WrapperMediaTransportFactory>(&first_);
  }

  std::unique_ptr<MediaTransportFactory> second_factory() {
    return absl::make_unique<WrapperMediaTransportFactory>(&second_);
  }

  void SetState(MediaTransportState state) {
    first_.SetState(state);
    second_.SetState(state);
  }

  void FlushAsyncInvokes() {
    first_.FlushAsyncInvokes();
    second_.FlushAsyncInvokes();
  }

  Stats FirstStats() { return first_.GetStats(); }
  Stats SecondStats() { return second_.GetStats(); }

 private:
  class LoopbackMediaTransport : public MediaTransportInterface {
   public:
    LoopbackMediaTransport(rtc::Thread* thread, LoopbackMediaTransport* other)
        : thread_(thread), other_(other) {}

    ~LoopbackMediaTransport() {
      rtc::CritScope lock(&sink_lock_);
      RTC_CHECK(sink_ == nullptr);
      RTC_CHECK(data_sink_ == nullptr);
    }

    RTCError SendAudioFrame(uint64_t channel_id,
                            MediaTransportEncodedAudioFrame frame) override {
      {
        rtc::CritScope lock(&stats_lock_);
        ++stats_.sent_audio_frames;
      }
      invoker_.AsyncInvoke<void>(RTC_FROM_HERE, thread_,
                                 [this, channel_id, frame] {
                                   other_->OnData(channel_id, std::move(frame));
                                 });
      return RTCError::OK();
    };

    RTCError SendVideoFrame(
        uint64_t channel_id,
        const MediaTransportEncodedVideoFrame& frame) override {
      return RTCError::OK();
    }

    RTCError RequestKeyFrame(uint64_t channel_id) override {
      return RTCError::OK();
    }

    void SetReceiveAudioSink(MediaTransportAudioSinkInterface* sink) override {
      rtc::CritScope lock(&sink_lock_);
      if (sink) {
        RTC_CHECK(sink_ == nullptr);
      }
      sink_ = sink;
    }

    void SetReceiveVideoSink(MediaTransportVideoSinkInterface* sink) override {}

    void SetMediaTransportStateCallback(
        MediaTransportStateCallback* callback) override {
      rtc::CritScope lock(&sink_lock_);
      state_callback_ = callback;
      invoker_.AsyncInvoke<void>(RTC_FROM_HERE, thread_, [this] {
        RTC_DCHECK_RUN_ON(thread_);
        OnStateChanged();
      });
    }

    RTCError SendData(int channel_id,
                      const SendDataParams& params,
                      const rtc::CopyOnWriteBuffer& buffer) override {
      invoker_.AsyncInvoke<void>(
          RTC_FROM_HERE, thread_, [this, channel_id, params, buffer] {
            other_->OnData(channel_id, params.type, buffer);
          });
      return RTCError::OK();
    }

    RTCError CloseChannel(int channel_id) override {
      invoker_.AsyncInvoke<void>(RTC_FROM_HERE, thread_, [this, channel_id] {
        other_->OnRemoteCloseChannel(channel_id);
        rtc::CritScope lock(&sink_lock_);
        if (data_sink_) {
          data_sink_->OnChannelClosed(channel_id);
        }
      });
      return RTCError::OK();
    }

    void SetDataSink(DataChannelSink* sink) override {
      rtc::CritScope lock(&sink_lock_);
      data_sink_ = sink;
    }

    void SetState(MediaTransportState state) {
      invoker_.AsyncInvoke<void>(RTC_FROM_HERE, thread_, [this, state] {
        RTC_DCHECK_RUN_ON(thread_);
        state_ = state;
        OnStateChanged();
      });
    }

    void FlushAsyncInvokes() { invoker_.Flush(thread_); }

    Stats GetStats() {
      rtc::CritScope lock(&stats_lock_);
      return stats_;
    }

   private:
    void OnData(uint64_t channel_id, MediaTransportEncodedAudioFrame frame) {
      {
        rtc::CritScope lock(&sink_lock_);
        if (sink_) {
          sink_->OnData(channel_id, frame);
        }
      }
      {
        rtc::CritScope lock(&stats_lock_);
        ++stats_.received_audio_frames;
      }
    }

    void OnData(int channel_id,
                DataMessageType type,
                const rtc::CopyOnWriteBuffer& buffer) {
      rtc::CritScope lock(&sink_lock_);
      if (data_sink_) {
        data_sink_->OnDataReceived(channel_id, type, buffer);
      }
    }

    void OnRemoteCloseChannel(int channel_id) {
      rtc::CritScope lock(&sink_lock_);
      if (data_sink_) {
        data_sink_->OnChannelClosing(channel_id);
        data_sink_->OnChannelClosed(channel_id);
      }
    }

    void OnStateChanged() RTC_RUN_ON(thread_) {
      rtc::CritScope lock(&sink_lock_);
      if (state_callback_) {
        state_callback_->OnStateChanged(state_);
      }
    }

    rtc::Thread* const thread_;
    rtc::CriticalSection sink_lock_;
    rtc::CriticalSection stats_lock_;

    MediaTransportAudioSinkInterface* sink_ RTC_GUARDED_BY(sink_lock_) =
        nullptr;
    DataChannelSink* data_sink_ RTC_GUARDED_BY(sink_lock_) = nullptr;
    MediaTransportStateCallback* state_callback_ RTC_GUARDED_BY(sink_lock_) =
        nullptr;

    MediaTransportState state_ RTC_GUARDED_BY(thread_) =
        MediaTransportState::kPending;

    LoopbackMediaTransport* const other_;

    Stats stats_ RTC_GUARDED_BY(stats_lock_);

    rtc::AsyncInvoker invoker_;
  };

  LoopbackMediaTransport first_;
  LoopbackMediaTransport second_;
};

}  // namespace webrtc

#endif  // API_TEST_LOOPBACK_MEDIA_TRANSPORT_H_
