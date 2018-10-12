/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/neteq/tools/neteq_test_factory.h"

#include <errno.h>
#include <limits.h>  // For ULONG_MAX returned by strtoul.
#include <stdio.h>
#include <stdlib.h>  // For strtoul.
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "absl/memory/memory.h"
#include "modules/audio_coding/neteq/include/neteq.h"
#include "modules/audio_coding/neteq/tools/fake_decode_from_file.h"
#include "modules/audio_coding/neteq/tools/input_audio_file.h"
#include "modules/audio_coding/neteq/tools/neteq_delay_analyzer.h"
#include "modules/audio_coding/neteq/tools/neteq_event_log_input.h"
#include "modules/audio_coding/neteq/tools/neteq_packet_source_input.h"
#include "modules/audio_coding/neteq/tools/neteq_replacement_input.h"
#include "modules/audio_coding/neteq/tools/neteq_stats_getter.h"
#include "modules/audio_coding/neteq/tools/neteq_stats_plotter.h"
#include "modules/audio_coding/neteq/tools/neteq_test.h"
#include "modules/audio_coding/neteq/tools/output_audio_file.h"
#include "modules/audio_coding/neteq/tools/output_wav_file.h"
#include "modules/audio_coding/neteq/tools/rtp_file_source.h"
#include "rtc_base/checks.h"
#include "rtc_base/flags.h"
#include "test/testsupport/fileutils.h"

namespace webrtc {
namespace test {
namespace {

// Parses the input string for a valid SSRC (at the start of the string). If a
// valid SSRC is found, it is written to the output variable |ssrc|, and true is
// returned. Otherwise, false is returned.
bool ParseSsrc(const std::string& str, uint32_t* ssrc) {
  if (str.empty())
    return true;
  int base = 10;
  // Look for "0x" or "0X" at the start and change base to 16 if found.
  if ((str.compare(0, 2, "0x") == 0) || (str.compare(0, 2, "0X") == 0))
    base = 16;
  errno = 0;
  char* end_ptr;
  unsigned long value = strtoul(str.c_str(), &end_ptr, base);  // NOLINT
  if (value == ULONG_MAX && errno == ERANGE)
    return false;  // Value out of range for unsigned long.
  if (sizeof(unsigned long) > sizeof(uint32_t) && value > 0xFFFFFFFF)  // NOLINT
    return false;  // Value out of range for uint32_t.
  if (end_ptr - str.c_str() < static_cast<ptrdiff_t>(str.length()))
    return false;  // Part of the string was not parsed.
  *ssrc = static_cast<uint32_t>(value);
  return true;
}

// Flag validators.
bool ValidatePayloadType(int value) {
  if (value >= 0 && value <= 127)  // Value is ok.
    return true;
  printf("Payload type must be between 0 and 127, not %d\n",
         static_cast<int>(value));
  return false;
}

bool ValidateSsrcValue(const std::string& str) {
  uint32_t dummy_ssrc;
  if (ParseSsrc(str, &dummy_ssrc))  // Value is ok.
    return true;
  printf("Invalid SSRC: %s\n", str.c_str());
  return false;
}

static bool ValidateExtensionId(int value) {
  if (value > 0 && value <= 255)  // Value is ok.
    return true;
  printf("Extension ID must be between 1 and 255, not %d\n",
         static_cast<int>(value));
  return false;
}

// Define command line flags.
DEFINE_int(pcmu, 0, "RTP payload type for PCM-u");
DEFINE_int(pcma, 8, "RTP payload type for PCM-a");
DEFINE_int(ilbc, 102, "RTP payload type for iLBC");
DEFINE_int(isac, 103, "RTP payload type for iSAC");
DEFINE_int(isac_swb, 104, "RTP payload type for iSAC-swb (32 kHz)");
DEFINE_int(opus, 111, "RTP payload type for Opus");
DEFINE_int(pcm16b, 93, "RTP payload type for PCM16b-nb (8 kHz)");
DEFINE_int(pcm16b_wb, 94, "RTP payload type for PCM16b-wb (16 kHz)");
DEFINE_int(pcm16b_swb32, 95, "RTP payload type for PCM16b-swb32 (32 kHz)");
DEFINE_int(pcm16b_swb48, 96, "RTP payload type for PCM16b-swb48 (48 kHz)");
DEFINE_int(g722, 9, "RTP payload type for G.722");
DEFINE_int(avt, 106, "RTP payload type for AVT/DTMF (8 kHz)");
DEFINE_int(avt_16, 114, "RTP payload type for AVT/DTMF (16 kHz)");
DEFINE_int(avt_32, 115, "RTP payload type for AVT/DTMF (32 kHz)");
DEFINE_int(avt_48, 116, "RTP payload type for AVT/DTMF (48 kHz)");
DEFINE_int(red, 117, "RTP payload type for redundant audio (RED)");
DEFINE_int(cn_nb, 13, "RTP payload type for comfort noise (8 kHz)");
DEFINE_int(cn_wb, 98, "RTP payload type for comfort noise (16 kHz)");
DEFINE_int(cn_swb32, 99, "RTP payload type for comfort noise (32 kHz)");
DEFINE_int(cn_swb48, 100, "RTP payload type for comfort noise (48 kHz)");
DEFINE_string(replacement_audio_file,
              "",
              "A PCM file that will be used to populate "
              "dummy"
              " RTP packets");
DEFINE_string(ssrc,
              "",
              "Only use packets with this SSRC (decimal or hex, the latter "
              "starting with 0x)");
DEFINE_int(audio_level, 1, "Extension ID for audio level (RFC 6464)");
DEFINE_int(abs_send_time, 3, "Extension ID for absolute sender time");
DEFINE_int(transport_seq_no, 5, "Extension ID for transport sequence number");
DEFINE_int(video_content_type, 7, "Extension ID for video content type");
DEFINE_int(video_timing, 8, "Extension ID for video timing");
DEFINE_bool(matlabplot,
            false,
            "Generates a matlab script for plotting the delay profile");
DEFINE_bool(pythonplot,
            false,
            "Generates a python script for plotting the delay profile");
DEFINE_bool(concealment_events, false, "Prints concealment events");

// Maps a codec type to a printable name string.
std::string CodecName(NetEqDecoder codec) {
  switch (codec) {
    case NetEqDecoder::kDecoderPCMu:
      return "PCM-u";
    case NetEqDecoder::kDecoderPCMa:
      return "PCM-a";
    case NetEqDecoder::kDecoderILBC:
      return "iLBC";
    case NetEqDecoder::kDecoderISAC:
      return "iSAC";
    case NetEqDecoder::kDecoderISACswb:
      return "iSAC-swb (32 kHz)";
    case NetEqDecoder::kDecoderOpus:
      return "Opus";
    case NetEqDecoder::kDecoderPCM16B:
      return "PCM16b-nb (8 kHz)";
    case NetEqDecoder::kDecoderPCM16Bwb:
      return "PCM16b-wb (16 kHz)";
    case NetEqDecoder::kDecoderPCM16Bswb32kHz:
      return "PCM16b-swb32 (32 kHz)";
    case NetEqDecoder::kDecoderPCM16Bswb48kHz:
      return "PCM16b-swb48 (48 kHz)";
    case NetEqDecoder::kDecoderG722:
      return "G.722";
    case NetEqDecoder::kDecoderRED:
      return "redundant audio (RED)";
    case NetEqDecoder::kDecoderAVT:
      return "AVT/DTMF (8 kHz)";
    case NetEqDecoder::kDecoderAVT16kHz:
      return "AVT/DTMF (16 kHz)";
    case NetEqDecoder::kDecoderAVT32kHz:
      return "AVT/DTMF (32 kHz)";
    case NetEqDecoder::kDecoderAVT48kHz:
      return "AVT/DTMF (48 kHz)";
    case NetEqDecoder::kDecoderCNGnb:
      return "comfort noise (8 kHz)";
    case NetEqDecoder::kDecoderCNGwb:
      return "comfort noise (16 kHz)";
    case NetEqDecoder::kDecoderCNGswb32kHz:
      return "comfort noise (32 kHz)";
    case NetEqDecoder::kDecoderCNGswb48kHz:
      return "comfort noise (48 kHz)";
    default:
      FATAL();
      return "undefined";
  }
}

void PrintCodecMappingEntry(NetEqDecoder codec, int flag) {
  std::cout << CodecName(codec) << ": " << flag << std::endl;
}

void PrintCodecMapping() {
  PrintCodecMappingEntry(NetEqDecoder::kDecoderPCMu, FLAG_pcmu);
  PrintCodecMappingEntry(NetEqDecoder::kDecoderPCMa, FLAG_pcma);
  PrintCodecMappingEntry(NetEqDecoder::kDecoderILBC, FLAG_ilbc);
  PrintCodecMappingEntry(NetEqDecoder::kDecoderISAC, FLAG_isac);
  PrintCodecMappingEntry(NetEqDecoder::kDecoderISACswb, FLAG_isac_swb);
  PrintCodecMappingEntry(NetEqDecoder::kDecoderOpus, FLAG_opus);
  PrintCodecMappingEntry(NetEqDecoder::kDecoderPCM16B, FLAG_pcm16b);
  PrintCodecMappingEntry(NetEqDecoder::kDecoderPCM16Bwb, FLAG_pcm16b_wb);
  PrintCodecMappingEntry(NetEqDecoder::kDecoderPCM16Bswb32kHz,
                         FLAG_pcm16b_swb32);
  PrintCodecMappingEntry(NetEqDecoder::kDecoderPCM16Bswb48kHz,
                         FLAG_pcm16b_swb48);
  PrintCodecMappingEntry(NetEqDecoder::kDecoderG722, FLAG_g722);
  PrintCodecMappingEntry(NetEqDecoder::kDecoderAVT, FLAG_avt);
  PrintCodecMappingEntry(NetEqDecoder::kDecoderAVT16kHz, FLAG_avt_16);
  PrintCodecMappingEntry(NetEqDecoder::kDecoderAVT32kHz, FLAG_avt_32);
  PrintCodecMappingEntry(NetEqDecoder::kDecoderAVT48kHz, FLAG_avt_48);
  PrintCodecMappingEntry(NetEqDecoder::kDecoderRED, FLAG_red);
  PrintCodecMappingEntry(NetEqDecoder::kDecoderCNGnb, FLAG_cn_nb);
  PrintCodecMappingEntry(NetEqDecoder::kDecoderCNGwb, FLAG_cn_wb);
  PrintCodecMappingEntry(NetEqDecoder::kDecoderCNGswb32kHz, FLAG_cn_swb32);
  PrintCodecMappingEntry(NetEqDecoder::kDecoderCNGswb48kHz, FLAG_cn_swb48);
}

absl::optional<int> CodecSampleRate(uint8_t payload_type) {
  if (payload_type == FLAG_pcmu || payload_type == FLAG_pcma ||
      payload_type == FLAG_ilbc || payload_type == FLAG_pcm16b ||
      payload_type == FLAG_cn_nb || payload_type == FLAG_avt)
    return 8000;
  if (payload_type == FLAG_isac || payload_type == FLAG_pcm16b_wb ||
      payload_type == FLAG_g722 || payload_type == FLAG_cn_wb ||
      payload_type == FLAG_avt_16)
    return 16000;
  if (payload_type == FLAG_isac_swb || payload_type == FLAG_pcm16b_swb32 ||
      payload_type == FLAG_cn_swb32 || payload_type == FLAG_avt_32)
    return 32000;
  if (payload_type == FLAG_opus || payload_type == FLAG_pcm16b_swb48 ||
      payload_type == FLAG_cn_swb48 || payload_type == FLAG_avt_48)
    return 48000;
  if (payload_type == FLAG_red)
    return 0;
  return absl::nullopt;
}

}  // namespace

// A callback class which prints whenver the inserted packet stream changes
// the SSRC.
class SsrcSwitchDetector : public NetEqPostInsertPacket {
 public:
  // Takes a pointer to another callback object, which will be invoked after
  // this object finishes. This does not transfer ownership, and null is a
  // valid value.
  explicit SsrcSwitchDetector(NetEqPostInsertPacket* other_callback)
      : other_callback_(other_callback) {}

  void AfterInsertPacket(const NetEqInput::PacketData& packet,
                         NetEq* neteq) override {
    if (last_ssrc_ && packet.header.ssrc != *last_ssrc_) {
      std::cout << "Changing streams from 0x" << std::hex << *last_ssrc_
                << " to 0x" << std::hex << packet.header.ssrc << std::dec
                << " (payload type "
                << static_cast<int>(packet.header.payloadType) << ")"
                << std::endl;
    }
    last_ssrc_ = packet.header.ssrc;
    if (other_callback_) {
      other_callback_->AfterInsertPacket(packet, neteq);
    }
  }

 private:
  NetEqPostInsertPacket* other_callback_;
  absl::optional<uint32_t> last_ssrc_;
};

NetEqTestFactory::NetEqTestFactory() = default;

NetEqTestFactory::~NetEqTestFactory() = default;

void NetEqTestFactory::PrintCodecMap() {
  PrintCodecMapping();
}

std::unique_ptr<NetEqTest> NetEqTestFactory::InitializeTest(
    std::string input_file_name,
    std::string output_file_name) {
  RTC_CHECK(ValidatePayloadType(FLAG_pcmu));
  RTC_CHECK(ValidatePayloadType(FLAG_pcma));
  RTC_CHECK(ValidatePayloadType(FLAG_ilbc));
  RTC_CHECK(ValidatePayloadType(FLAG_isac));
  RTC_CHECK(ValidatePayloadType(FLAG_isac_swb));
  RTC_CHECK(ValidatePayloadType(FLAG_opus));
  RTC_CHECK(ValidatePayloadType(FLAG_pcm16b));
  RTC_CHECK(ValidatePayloadType(FLAG_pcm16b_wb));
  RTC_CHECK(ValidatePayloadType(FLAG_pcm16b_swb32));
  RTC_CHECK(ValidatePayloadType(FLAG_pcm16b_swb48));
  RTC_CHECK(ValidatePayloadType(FLAG_g722));
  RTC_CHECK(ValidatePayloadType(FLAG_avt));
  RTC_CHECK(ValidatePayloadType(FLAG_avt_16));
  RTC_CHECK(ValidatePayloadType(FLAG_avt_32));
  RTC_CHECK(ValidatePayloadType(FLAG_avt_48));
  RTC_CHECK(ValidatePayloadType(FLAG_red));
  RTC_CHECK(ValidatePayloadType(FLAG_cn_nb));
  RTC_CHECK(ValidatePayloadType(FLAG_cn_wb));
  RTC_CHECK(ValidatePayloadType(FLAG_cn_swb32));
  RTC_CHECK(ValidatePayloadType(FLAG_cn_swb48));
  RTC_CHECK(ValidateSsrcValue(FLAG_ssrc));
  RTC_CHECK(ValidateExtensionId(FLAG_audio_level));
  RTC_CHECK(ValidateExtensionId(FLAG_abs_send_time));
  RTC_CHECK(ValidateExtensionId(FLAG_transport_seq_no));
  RTC_CHECK(ValidateExtensionId(FLAG_video_content_type));
  RTC_CHECK(ValidateExtensionId(FLAG_video_timing));

  // Gather RTP header extensions in a map.
  NetEqPacketSourceInput::RtpHeaderExtensionMap rtp_ext_map = {
      {FLAG_audio_level, kRtpExtensionAudioLevel},
      {FLAG_abs_send_time, kRtpExtensionAbsoluteSendTime},
      {FLAG_transport_seq_no, kRtpExtensionTransportSequenceNumber},
      {FLAG_video_content_type, kRtpExtensionVideoContentType},
      {FLAG_video_timing, kRtpExtensionVideoTiming}};

  std::unique_ptr<NetEqInput> input;
  if (RtpFileSource::ValidRtpDump(input_file_name) ||
      RtpFileSource::ValidPcap(input_file_name)) {
    input.reset(new NetEqRtpDumpInput(input_file_name, rtp_ext_map));
  } else {
    input.reset(new NetEqEventLogInput(input_file_name, rtp_ext_map));
  }

  std::cout << "Input file: " << input_file_name << std::endl;
  RTC_CHECK(input) << "Cannot open input file";
  RTC_CHECK(!input->ended()) << "Input file is empty";

  // Check if an SSRC value was provided.
  if (strlen(FLAG_ssrc) > 0) {
    uint32_t ssrc;
    RTC_CHECK(ParseSsrc(FLAG_ssrc, &ssrc)) << "Flag verification has failed.";
    static_cast<NetEqPacketSourceInput*>(input.get())->SelectSsrc(ssrc);
  }

  // Check the sample rate.
  absl::optional<int> sample_rate_hz;
  std::set<std::pair<int, uint32_t>> discarded_pt_and_ssrc;
  while (absl::optional<RTPHeader> first_rtp_header = input->NextHeader()) {
    RTC_DCHECK(first_rtp_header);
    sample_rate_hz = CodecSampleRate(first_rtp_header->payloadType);
    if (sample_rate_hz) {
      std::cout << "Found valid packet with payload type "
                << static_cast<int>(first_rtp_header->payloadType)
                << " and SSRC 0x" << std::hex << first_rtp_header->ssrc
                << std::dec << std::endl;
      break;
    }
    // Discard this packet and move to the next. Keep track of discarded payload
    // types and SSRCs.
    discarded_pt_and_ssrc.emplace(first_rtp_header->payloadType,
                                  first_rtp_header->ssrc);
    input->PopPacket();
  }
  if (!discarded_pt_and_ssrc.empty()) {
    std::cout << "Discarded initial packets with the following payload types "
                 "and SSRCs:"
              << std::endl;
    for (const auto& d : discarded_pt_and_ssrc) {
      std::cout << "PT " << d.first << "; SSRC 0x" << std::hex
                << static_cast<int>(d.second) << std::dec << std::endl;
    }
  }
  if (!sample_rate_hz) {
    std::cout << "Cannot find any packets with known payload types"
              << std::endl;
    RTC_NOTREACHED();
  }

  // Open the output file now that we know the sample rate. (Rate is only needed
  // for wav files.)
  std::unique_ptr<AudioSink> output;
  if (output_file_name.size() >= 4 &&
      output_file_name.substr(output_file_name.size() - 4) == ".wav") {
    // Open a wav file.
    output.reset(new OutputWavFile(output_file_name, *sample_rate_hz));
  } else {
    // Open a pcm file.
    output.reset(new OutputAudioFile(output_file_name));
  }

  std::cout << "Output file: " << output_file_name << std::endl;

  NetEqTest::DecoderMap codecs = {
    {FLAG_pcmu, std::make_pair(NetEqDecoder::kDecoderPCMu, "pcmu")},
    {FLAG_pcma, std::make_pair(NetEqDecoder::kDecoderPCMa, "pcma")},
#ifdef WEBRTC_CODEC_ILBC
    {FLAG_ilbc, std::make_pair(NetEqDecoder::kDecoderILBC, "ilbc")},
#endif
    {FLAG_isac, std::make_pair(NetEqDecoder::kDecoderISAC, "isac")},
#if !defined(WEBRTC_ANDROID)
    {FLAG_isac_swb, std::make_pair(NetEqDecoder::kDecoderISACswb, "isac-swb")},
#endif
#ifdef WEBRTC_CODEC_OPUS
    {FLAG_opus, std::make_pair(NetEqDecoder::kDecoderOpus, "opus")},
#endif
    {FLAG_pcm16b, std::make_pair(NetEqDecoder::kDecoderPCM16B, "pcm16-nb")},
    {FLAG_pcm16b_wb,
     std::make_pair(NetEqDecoder::kDecoderPCM16Bwb, "pcm16-wb")},
    {FLAG_pcm16b_swb32,
     std::make_pair(NetEqDecoder::kDecoderPCM16Bswb32kHz, "pcm16-swb32")},
    {FLAG_pcm16b_swb48,
     std::make_pair(NetEqDecoder::kDecoderPCM16Bswb48kHz, "pcm16-swb48")},
    {FLAG_g722, std::make_pair(NetEqDecoder::kDecoderG722, "g722")},
    {FLAG_avt, std::make_pair(NetEqDecoder::kDecoderAVT, "avt")},
    {FLAG_avt_16, std::make_pair(NetEqDecoder::kDecoderAVT16kHz, "avt-16")},
    {FLAG_avt_32, std::make_pair(NetEqDecoder::kDecoderAVT32kHz, "avt-32")},
    {FLAG_avt_48, std::make_pair(NetEqDecoder::kDecoderAVT48kHz, "avt-48")},
    {FLAG_red, std::make_pair(NetEqDecoder::kDecoderRED, "red")},
    {FLAG_cn_nb, std::make_pair(NetEqDecoder::kDecoderCNGnb, "cng-nb")},
    {FLAG_cn_wb, std::make_pair(NetEqDecoder::kDecoderCNGwb, "cng-wb")},
    {FLAG_cn_swb32,
     std::make_pair(NetEqDecoder::kDecoderCNGswb32kHz, "cng-swb32")},
    {FLAG_cn_swb48,
     std::make_pair(NetEqDecoder::kDecoderCNGswb48kHz, "cng-swb48")}
  };

  // Check if a replacement audio file was provided.
  if (strlen(FLAG_replacement_audio_file) > 0) {
    // Find largest unused payload type.
    int replacement_pt = 127;
    while (!(codecs.find(replacement_pt) == codecs.end() &&
             ext_codecs_.find(replacement_pt) == ext_codecs_.end())) {
      --replacement_pt;
      RTC_CHECK_GE(replacement_pt, 0);
    }

    auto std_set_int32_to_uint8 = [](const std::set<int32_t>& a) {
      std::set<uint8_t> b;
      for (auto& x : a) {
        b.insert(static_cast<uint8_t>(x));
      }
      return b;
    };

    std::set<uint8_t> cn_types = std_set_int32_to_uint8(
        {FLAG_cn_nb, FLAG_cn_wb, FLAG_cn_swb32, FLAG_cn_swb48});
    std::set<uint8_t> forbidden_types = std_set_int32_to_uint8(
        {FLAG_g722, FLAG_red, FLAG_avt, FLAG_avt_16, FLAG_avt_32, FLAG_avt_48});
    input.reset(new NetEqReplacementInput(std::move(input), replacement_pt,
                                          cn_types, forbidden_types));

    replacement_decoder_.reset(new FakeDecodeFromFile(
        std::unique_ptr<InputAudioFile>(
            new InputAudioFile(FLAG_replacement_audio_file)),
        48000, false));
    NetEqTest::ExternalDecoderInfo ext_dec_info = {
        replacement_decoder_.get(), NetEqDecoder::kDecoderArbitrary,
        "replacement codec"};
    ext_codecs_[replacement_pt] = ext_dec_info;
  }

  NetEqTest::Callbacks callbacks;
  stats_plotter_.reset(new NetEqStatsPlotter(FLAG_matlabplot, FLAG_pythonplot,
                                             FLAG_concealment_events,
                                             output_file_name));

  ssrc_switch_detector_.reset(
      new SsrcSwitchDetector(stats_plotter_->stats_getter()->delay_analyzer()));
  callbacks.post_insert_packet = ssrc_switch_detector_.get();
  callbacks.get_audio_callback = stats_plotter_->stats_getter();
  callbacks.simulation_ended_callback = stats_plotter_.get();
  NetEq::Config config;
  config.sample_rate_hz = *sample_rate_hz;
  return absl::make_unique<NetEqTest>(config, codecs, ext_codecs_,
                                      std::move(input), std::move(output),
                                      callbacks);
}

}  // namespace test
}  // namespace webrtc
