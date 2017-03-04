/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/test/rtp_player.h"

#include <stdio.h>

#include <cstdlib>
#include <map>
#include <memory>

#include "webrtc/base/constructormagic.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_header_parser.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_payload_registry.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_receiver.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp.h"
#include "webrtc/modules/video_coding/internal_defines.h"
#include "webrtc/modules/video_coding/test/test_util.h"
#include "webrtc/system_wrappers/include/clock.h"
#include "webrtc/system_wrappers/include/critical_section_wrapper.h"
#include "webrtc/test/rtp_file_reader.h"

#if 1
#define DEBUG_LOG1(text, arg)
#else
#define DEBUG_LOG1(text, arg) (printf(text "\n", arg))
#endif

namespace webrtc {
namespace rtpplayer {

enum {
  kMaxPacketBufferSize = 4096,
  kDefaultTransmissionTimeOffsetExtensionId = 2
};

class RawRtpPacket {
 public:
  RawRtpPacket(const uint8_t* data,
               size_t length,
               uint32_t ssrc,
               uint16_t seq_num)
      : data_(new uint8_t[length]),
        length_(length),
        resend_time_ms_(-1),
        ssrc_(ssrc),
        seq_num_(seq_num) {
    assert(data);
    memcpy(data_.get(), data, length_);
  }

  const uint8_t* data() const { return data_.get(); }
  size_t length() const { return length_; }
  int64_t resend_time_ms() const { return resend_time_ms_; }
  void set_resend_time_ms(int64_t timeMs) { resend_time_ms_ = timeMs; }
  uint32_t ssrc() const { return ssrc_; }
  uint16_t seq_num() const { return seq_num_; }

 private:
  std::unique_ptr<uint8_t[]> data_;
  size_t length_;
  int64_t resend_time_ms_;
  uint32_t ssrc_;
  uint16_t seq_num_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(RawRtpPacket);
};

class LostPackets {
 public:
  LostPackets(Clock* clock, int64_t rtt_ms)
      : crit_sect_(CriticalSectionWrapper::CreateCriticalSection()),
        debug_file_(fopen("PacketLossDebug.txt", "w")),
        loss_count_(0),
        packets_(),
        clock_(clock),
        rtt_ms_(rtt_ms) {
    assert(clock);
  }

  ~LostPackets() {
    if (debug_file_) {
      fclose(debug_file_);
      debug_file_ = NULL;
    }
    while (!packets_.empty()) {
      delete packets_.back();
      packets_.pop_back();
    }
  }

  void AddPacket(RawRtpPacket* packet) {
    assert(packet);
    printf("Throw:  %08x:%u\n", packet->ssrc(), packet->seq_num());
    CriticalSectionScoped cs(crit_sect_.get());
    if (debug_file_) {
      fprintf(debug_file_, "%u Lost packet: %u\n", loss_count_,
              packet->seq_num());
    }
    packets_.push_back(packet);
    loss_count_++;
  }

  void SetResendTime(uint32_t ssrc, int16_t resendSeqNum) {
    int64_t resend_time_ms = clock_->TimeInMilliseconds() + rtt_ms_;
    int64_t now_ms = clock_->TimeInMilliseconds();
    CriticalSectionScoped cs(crit_sect_.get());
    for (RtpPacketIterator it = packets_.begin(); it != packets_.end(); ++it) {
      RawRtpPacket* packet = *it;
      if (ssrc == packet->ssrc() && resendSeqNum == packet->seq_num() &&
          packet->resend_time_ms() + 10 < now_ms) {
        if (debug_file_) {
          fprintf(debug_file_, "Resend %u at %u\n", packet->seq_num(),
                  MaskWord64ToUWord32(resend_time_ms));
        }
        packet->set_resend_time_ms(resend_time_ms);
        return;
      }
    }
    // We may get here since the captured stream may itself be missing packets.
  }

  RawRtpPacket* NextPacketToResend(int64_t time_now) {
    CriticalSectionScoped cs(crit_sect_.get());
    for (RtpPacketIterator it = packets_.begin(); it != packets_.end(); ++it) {
      RawRtpPacket* packet = *it;
      if (time_now >= packet->resend_time_ms() &&
          packet->resend_time_ms() != -1) {
        packets_.erase(it);
        return packet;
      }
    }
    return NULL;
  }

  int NumberOfPacketsToResend() const {
    CriticalSectionScoped cs(crit_sect_.get());
    int count = 0;
    for (ConstRtpPacketIterator it = packets_.begin(); it != packets_.end();
         ++it) {
      if ((*it)->resend_time_ms() >= 0) {
        count++;
      }
    }
    return count;
  }

  void LogPacketResent(RawRtpPacket* packet) {
    int64_t now_ms = clock_->TimeInMilliseconds();
    CriticalSectionScoped cs(crit_sect_.get());
    if (debug_file_) {
      fprintf(debug_file_, "Resent %u at %u\n", packet->seq_num(),
              MaskWord64ToUWord32(now_ms));
    }
  }

  void Print() const {
    CriticalSectionScoped cs(crit_sect_.get());
    printf("Lost packets: %u\n", loss_count_);
    printf("Packets waiting to be resent: %d\n", NumberOfPacketsToResend());
    printf("Packets still lost: %zd\n", packets_.size());
    printf("Sequence numbers:\n");
    for (ConstRtpPacketIterator it = packets_.begin(); it != packets_.end();
         ++it) {
      printf("%u, ", (*it)->seq_num());
    }
    printf("\n");
  }

 private:
  typedef std::vector<RawRtpPacket*> RtpPacketList;
  typedef RtpPacketList::iterator RtpPacketIterator;
  typedef RtpPacketList::const_iterator ConstRtpPacketIterator;

  std::unique_ptr<CriticalSectionWrapper> crit_sect_;
  FILE* debug_file_;
  int loss_count_;
  RtpPacketList packets_;
  Clock* clock_;
  int64_t rtt_ms_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(LostPackets);
};

class SsrcHandlers {
 public:
  SsrcHandlers(PayloadSinkFactoryInterface* payload_sink_factory,
               const PayloadTypes& payload_types)
      : payload_sink_factory_(payload_sink_factory),
        payload_types_(payload_types),
        handlers_() {
    assert(payload_sink_factory);
  }

  ~SsrcHandlers() {
    while (!handlers_.empty()) {
      delete handlers_.begin()->second;
      handlers_.erase(handlers_.begin());
    }
  }

  int RegisterSsrc(uint32_t ssrc, LostPackets* lost_packets, Clock* clock) {
    if (handlers_.count(ssrc) > 0) {
      return 0;
    }
    DEBUG_LOG1("Registering handler for ssrc=%08x", ssrc);

    std::unique_ptr<Handler> handler(
        new Handler(ssrc, payload_types_, lost_packets));
    handler->payload_sink_.reset(payload_sink_factory_->Create(handler.get()));
    if (handler->payload_sink_.get() == NULL) {
      return -1;
    }

    RtpRtcp::Configuration configuration;
    configuration.clock = clock;
    configuration.audio = false;
    handler->rtp_module_.reset(RtpReceiver::CreateVideoReceiver(
        configuration.clock, handler->payload_sink_.get(), NULL,
        handler->rtp_payload_registry_.get()));
    if (handler->rtp_module_.get() == NULL) {
      return -1;
    }

    handler->rtp_header_parser_->RegisterRtpHeaderExtension(
        kRtpExtensionTransmissionTimeOffset,
        kDefaultTransmissionTimeOffsetExtensionId);

    for (PayloadTypesIterator it = payload_types_.begin();
         it != payload_types_.end(); ++it) {
      VideoCodec codec;
      memset(&codec, 0, sizeof(codec));
      strncpy(codec.plName, it->name().c_str(), sizeof(codec.plName) - 1);
      codec.plType = it->payload_type();
      codec.codecType = it->codec_type();
      if (handler->rtp_module_->RegisterReceivePayload(
              codec.plName, codec.plType, 90000, 0, codec.maxBitrate) < 0) {
        return -1;
      }
    }

    handlers_[ssrc] = handler.release();
    return 0;
  }

  void IncomingPacket(const uint8_t* data, size_t length) {
    for (HandlerMapIt it = handlers_.begin(); it != handlers_.end(); ++it) {
      if (!it->second->rtp_header_parser_->IsRtcp(data, length)) {
        RTPHeader header;
        it->second->rtp_header_parser_->Parse(data, length, &header);
        PayloadUnion payload_specific;
        it->second->rtp_payload_registry_->GetPayloadSpecifics(
            header.payloadType, &payload_specific);
        it->second->rtp_module_->IncomingRtpPacket(header, data, length,
                                                   payload_specific, true);
      }
    }
  }

 private:
  class Handler : public RtpStreamInterface {
   public:
    Handler(uint32_t ssrc,
            const PayloadTypes& payload_types,
            LostPackets* lost_packets)
        : rtp_header_parser_(RtpHeaderParser::Create()),
          rtp_payload_registry_(new RTPPayloadRegistry()),
          rtp_module_(),
          payload_sink_(),
          ssrc_(ssrc),
          payload_types_(payload_types),
          lost_packets_(lost_packets) {
      assert(lost_packets);
    }
    virtual ~Handler() {}

    virtual void ResendPackets(const uint16_t* sequence_numbers,
                               uint16_t length) {
      assert(sequence_numbers);
      for (uint16_t i = 0; i < length; i++) {
        lost_packets_->SetResendTime(ssrc_, sequence_numbers[i]);
      }
    }

    virtual uint32_t ssrc() const { return ssrc_; }
    virtual const PayloadTypes& payload_types() const { return payload_types_; }

    std::unique_ptr<RtpHeaderParser> rtp_header_parser_;
    std::unique_ptr<RTPPayloadRegistry> rtp_payload_registry_;
    std::unique_ptr<RtpReceiver> rtp_module_;
    std::unique_ptr<PayloadSinkInterface> payload_sink_;

   private:
    uint32_t ssrc_;
    const PayloadTypes& payload_types_;
    LostPackets* lost_packets_;

    RTC_DISALLOW_COPY_AND_ASSIGN(Handler);
  };

  typedef std::map<uint32_t, Handler*> HandlerMap;
  typedef std::map<uint32_t, Handler*>::iterator HandlerMapIt;

  PayloadSinkFactoryInterface* payload_sink_factory_;
  PayloadTypes payload_types_;
  HandlerMap handlers_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(SsrcHandlers);
};

class RtpPlayerImpl : public RtpPlayerInterface {
 public:
  RtpPlayerImpl(PayloadSinkFactoryInterface* payload_sink_factory,
                const PayloadTypes& payload_types,
                Clock* clock,
                std::unique_ptr<test::RtpFileReader>* packet_source,
                float loss_rate,
                int64_t rtt_ms,
                bool reordering)
      : ssrc_handlers_(payload_sink_factory, payload_types),
        clock_(clock),
        next_rtp_time_(0),
        first_packet_(true),
        first_packet_rtp_time_(0),
        first_packet_time_ms_(0),
        loss_rate_(loss_rate),
        lost_packets_(clock, rtt_ms),
        resend_packet_count_(0),
        no_loss_startup_(100),
        end_of_file_(false),
        reordering_(false),
        reorder_buffer_() {
    assert(clock);
    assert(packet_source);
    assert(packet_source->get());
    packet_source_.swap(*packet_source);
    std::srand(321);
  }

  virtual ~RtpPlayerImpl() {}

  virtual int NextPacket(int64_t time_now) {
    // Send any packets ready to be resent.
    for (RawRtpPacket* packet = lost_packets_.NextPacketToResend(time_now);
         packet != NULL; packet = lost_packets_.NextPacketToResend(time_now)) {
      int ret = SendPacket(packet->data(), packet->length());
      if (ret > 0) {
        printf("Resend: %08x:%u\n", packet->ssrc(), packet->seq_num());
        lost_packets_.LogPacketResent(packet);
        resend_packet_count_++;
      }
      delete packet;
      if (ret < 0) {
        return ret;
      }
    }

    // Send any packets from packet source.
    if (!end_of_file_ && (TimeUntilNextPacket() == 0 || first_packet_)) {
      if (first_packet_) {
        if (!packet_source_->NextPacket(&next_packet_))
          return 0;
        first_packet_rtp_time_ = next_packet_.time_ms;
        first_packet_time_ms_ = clock_->TimeInMilliseconds();
        first_packet_ = false;
      }

      if (reordering_ && reorder_buffer_.get() == NULL) {
        reorder_buffer_.reset(
            new RawRtpPacket(next_packet_.data, next_packet_.length, 0, 0));
        return 0;
      }
      int ret = SendPacket(next_packet_.data, next_packet_.length);
      if (reorder_buffer_.get()) {
        SendPacket(reorder_buffer_->data(), reorder_buffer_->length());
        reorder_buffer_.reset(NULL);
      }
      if (ret < 0) {
        return ret;
      }

      if (!packet_source_->NextPacket(&next_packet_)) {
        end_of_file_ = true;
        return 0;
      } else if (next_packet_.length == 0) {
        return 0;
      }
    }

    if (end_of_file_ && lost_packets_.NumberOfPacketsToResend() == 0) {
      return 1;
    }
    return 0;
  }

  virtual uint32_t TimeUntilNextPacket() const {
    int64_t time_left = (next_rtp_time_ - first_packet_rtp_time_) -
                        (clock_->TimeInMilliseconds() - first_packet_time_ms_);
    if (time_left < 0) {
      return 0;
    }
    return static_cast<uint32_t>(time_left);
  }

  virtual void Print() const {
    printf("Resent packets: %u\n", resend_packet_count_);
    lost_packets_.Print();
  }

 private:
  int SendPacket(const uint8_t* data, size_t length) {
    assert(data);
    assert(length > 0);

    std::unique_ptr<RtpHeaderParser> rtp_header_parser(
        RtpHeaderParser::Create());
    if (!rtp_header_parser->IsRtcp(data, length)) {
      RTPHeader header;
      if (!rtp_header_parser->Parse(data, length, &header)) {
        return -1;
      }
      uint32_t ssrc = header.ssrc;
      if (ssrc_handlers_.RegisterSsrc(ssrc, &lost_packets_, clock_) < 0) {
        DEBUG_LOG1("Unable to register ssrc: %d", ssrc);
        return -1;
      }

      if (no_loss_startup_ > 0) {
        no_loss_startup_--;
      } else if ((std::rand() + 1.0) / (RAND_MAX + 1.0) <
                 loss_rate_) {  // NOLINT
        uint16_t seq_num = header.sequenceNumber;
        lost_packets_.AddPacket(new RawRtpPacket(data, length, ssrc, seq_num));
        DEBUG_LOG1("Dropped packet: %d!", header.header.sequenceNumber);
        return 0;
      }
    }

    ssrc_handlers_.IncomingPacket(data, length);
    return 1;
  }

  SsrcHandlers ssrc_handlers_;
  Clock* clock_;
  std::unique_ptr<test::RtpFileReader> packet_source_;
  test::RtpPacket next_packet_;
  uint32_t next_rtp_time_;
  bool first_packet_;
  int64_t first_packet_rtp_time_;
  int64_t first_packet_time_ms_;
  float loss_rate_;
  LostPackets lost_packets_;
  uint32_t resend_packet_count_;
  uint32_t no_loss_startup_;
  bool end_of_file_;
  bool reordering_;
  std::unique_ptr<RawRtpPacket> reorder_buffer_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(RtpPlayerImpl);
};

RtpPlayerInterface* Create(const std::string& input_filename,
                           PayloadSinkFactoryInterface* payload_sink_factory,
                           Clock* clock,
                           const PayloadTypes& payload_types,
                           float loss_rate,
                           int64_t rtt_ms,
                           bool reordering) {
  std::unique_ptr<test::RtpFileReader> packet_source(
      test::RtpFileReader::Create(test::RtpFileReader::kRtpDump,
                                  input_filename));
  if (packet_source.get() == NULL) {
    packet_source.reset(test::RtpFileReader::Create(test::RtpFileReader::kPcap,
                                                    input_filename));
    if (packet_source.get() == NULL) {
      return NULL;
    }
  }

  std::unique_ptr<RtpPlayerImpl> impl(
      new RtpPlayerImpl(payload_sink_factory, payload_types, clock,
                        &packet_source, loss_rate, rtt_ms, reordering));
  return impl.release();
}
}  // namespace rtpplayer
}  // namespace webrtc
