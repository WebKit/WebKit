/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_INCLUDE_AUDIO_CODING_MODULE_H_
#define MODULES_AUDIO_CODING_INCLUDE_AUDIO_CODING_MODULE_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/audio_codecs/audio_decoder_factory.h"
#include "api/audio_codecs/audio_encoder.h"
#include "common_types.h"  // NOLINT(build/include)
#include "modules/audio_coding/include/audio_coding_module_typedefs.h"
#include "modules/audio_coding/neteq/include/neteq.h"
#include "rtc_base/function_view.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

// forward declarations
struct CodecInst;
struct WebRtcRTPHeader;
class AudioDecoder;
class AudioEncoder;
class AudioFrame;
class RTPFragmentationHeader;

#define WEBRTC_10MS_PCM_AUDIO 960  // 16 bits super wideband 48 kHz

// Callback class used for sending data ready to be packetized
class AudioPacketizationCallback {
 public:
  virtual ~AudioPacketizationCallback() {}

  virtual int32_t SendData(FrameType frame_type,
                           uint8_t payload_type,
                           uint32_t timestamp,
                           const uint8_t* payload_data,
                           size_t payload_len_bytes,
                           const RTPFragmentationHeader* fragmentation) = 0;
};

// Callback class used for reporting VAD decision
class ACMVADCallback {
 public:
  virtual ~ACMVADCallback() {}

  virtual int32_t InFrameType(FrameType frame_type) = 0;
};

class AudioCodingModule {
 protected:
  AudioCodingModule() {}

 public:
  struct Config {
    explicit Config(
        rtc::scoped_refptr<AudioDecoderFactory> decoder_factory = nullptr);
    Config(const Config&);
    ~Config();

    NetEq::Config neteq_config;
    Clock* clock;
    rtc::scoped_refptr<AudioDecoderFactory> decoder_factory;
  };

  static AudioCodingModule* Create(const Config& config);
  virtual ~AudioCodingModule() = default;

  ///////////////////////////////////////////////////////////////////////////
  //   Utility functions
  //

  ///////////////////////////////////////////////////////////////////////////
  // uint8_t NumberOfCodecs()
  // Returns number of supported codecs.
  //
  // Return value:
  //   number of supported codecs.
  ///
  static int NumberOfCodecs();

  ///////////////////////////////////////////////////////////////////////////
  // int32_t Codec()
  // Get supported codec with list number.
  //
  // Input:
  //   -list_id             : list number.
  //
  // Output:
  //   -codec              : a structure where the parameters of the codec,
  //                         given by list number is written to.
  //
  // Return value:
  //   -1 if the list number (list_id) is invalid.
  //    0 if succeeded.
  //
  static int Codec(int list_id, CodecInst* codec);

  ///////////////////////////////////////////////////////////////////////////
  // int32_t Codec()
  // Get supported codec with the given codec name, sampling frequency, and
  // a given number of channels.
  //
  // Input:
  //   -payload_name       : name of the codec.
  //   -sampling_freq_hz   : sampling frequency of the codec. Note! for RED
  //                         a sampling frequency of -1 is a valid input.
  //   -channels           : number of channels ( 1 - mono, 2 - stereo).
  //
  // Output:
  //   -codec              : a structure where the function returns the
  //                         default parameters of the codec.
  //
  // Return value:
  //   -1 if no codec matches the given parameters.
  //    0 if succeeded.
  //
  static int Codec(const char* payload_name,
                   CodecInst* codec,
                   int sampling_freq_hz,
                   size_t channels);

  ///////////////////////////////////////////////////////////////////////////
  // int32_t Codec()
  //
  // Returns the list number of the given codec name, sampling frequency, and
  // a given number of channels.
  //
  // Input:
  //   -payload_name        : name of the codec.
  //   -sampling_freq_hz    : sampling frequency of the codec. Note! for RED
  //                          a sampling frequency of -1 is a valid input.
  //   -channels            : number of channels ( 1 - mono, 2 - stereo).
  //
  // Return value:
  //   if the codec is found, the index of the codec in the list,
  //   -1 if the codec is not found.
  //
  static int Codec(const char* payload_name,
                   int sampling_freq_hz,
                   size_t channels);

  ///////////////////////////////////////////////////////////////////////////
  //   Sender
  //

  // |modifier| is called exactly once with one argument: a pointer to the
  // unique_ptr that holds the current encoder (which is null if there is no
  // current encoder). For the duration of the call, |modifier| has exclusive
  // access to the unique_ptr; it may call the encoder, steal the encoder and
  // replace it with another encoder or with nullptr, etc.
  virtual void ModifyEncoder(
      rtc::FunctionView<void(std::unique_ptr<AudioEncoder>*)> modifier) = 0;

  // Utility method for simply replacing the existing encoder with a new one.
  void SetEncoder(std::unique_ptr<AudioEncoder> new_encoder) {
    ModifyEncoder([&](std::unique_ptr<AudioEncoder>* encoder) {
      *encoder = std::move(new_encoder);
    });
  }

  ///////////////////////////////////////////////////////////////////////////
  // int32_t SendCodec()
  // Get parameters for the codec currently registered as send codec.
  //
  // Return value:
  //   The send codec, or nothing if we don't have one
  //
  virtual absl::optional<CodecInst> SendCodec() const = 0;

  ///////////////////////////////////////////////////////////////////////////
  // Sets the bitrate to the specified value in bits/sec. If the value is not
  // supported by the codec, it will choose another appropriate value.
  //
  // This is only used in test code that rely on old ACM APIs.
  // TODO(minyue): Remove it when possible.
  virtual void SetBitRate(int bitrate_bps) = 0;

  // int32_t RegisterTransportCallback()
  // Register a transport callback which will be called to deliver
  // the encoded buffers whenever Process() is called and a
  // bit-stream is ready.
  //
  // Input:
  //   -transport          : pointer to the callback class
  //                         transport->SendData() is called whenever
  //                         Process() is called and bit-stream is ready
  //                         to deliver.
  //
  // Return value:
  //   -1 if the transport callback could not be registered
  //    0 if registration is successful.
  //
  virtual int32_t RegisterTransportCallback(
      AudioPacketizationCallback* transport) = 0;

  ///////////////////////////////////////////////////////////////////////////
  // int32_t Add10MsData()
  // Add 10MS of raw (PCM) audio data and encode it. If the sampling
  // frequency of the audio does not match the sampling frequency of the
  // current encoder ACM will resample the audio. If an encoded packet was
  // produced, it will be delivered via the callback object registered using
  // RegisterTransportCallback, and the return value from this function will
  // be the number of bytes encoded.
  //
  // Input:
  //   -audio_frame        : the input audio frame, containing raw audio
  //                         sampling frequency etc.
  //
  // Return value:
  //   >= 0   number of bytes encoded.
  //     -1   some error occurred.
  //
  virtual int32_t Add10MsData(const AudioFrame& audio_frame) = 0;

  ///////////////////////////////////////////////////////////////////////////
  // int SetPacketLossRate()
  // Sets expected packet loss rate for encoding. Some encoders provide packet
  // loss gnostic encoding to make stream less sensitive to packet losses,
  // through e.g., FEC. No effects on codecs that do not provide such encoding.
  //
  // Input:
  //   -packet_loss_rate   : expected packet loss rate (0 -- 100 inclusive).
  //
  // Return value
  //   -1 if failed to set packet loss rate,
  //   0 if succeeded.
  //
  // This is only used in test code that rely on old ACM APIs.
  // TODO(minyue): Remove it when possible.
  virtual int SetPacketLossRate(int packet_loss_rate) = 0;

  ///////////////////////////////////////////////////////////////////////////
  //   (VAD) Voice Activity Detection
  //

  ///////////////////////////////////////////////////////////////////////////
  // int32_t RegisterVADCallback()
  // Call this method to register a callback function which is called
  // any time that ACM encounters an empty frame. That is a frame which is
  // recognized inactive. Depending on the codec WebRtc VAD or internal codec
  // VAD is employed to identify a frame as active/inactive.
  //
  // Input:
  //   -vad_callback        : pointer to a callback function.
  //
  // Return value:
  //   -1 if failed to register the callback function.
  //    0 if the callback function is registered successfully.
  //
  virtual int32_t RegisterVADCallback(ACMVADCallback* vad_callback) = 0;

  ///////////////////////////////////////////////////////////////////////////
  //   Receiver
  //

  ///////////////////////////////////////////////////////////////////////////
  // int32_t InitializeReceiver()
  // Any decoder-related state of ACM will be initialized to the
  // same state when ACM is created. This will not interrupt or
  // effect encoding functionality of ACM. ACM would lose all the
  // decoding-related settings by calling this function.
  // For instance, all registered codecs are deleted and have to be
  // registered again.
  //
  // Return value:
  //   -1 if failed to initialize,
  //    0 if succeeded.
  //
  virtual int32_t InitializeReceiver() = 0;

  ///////////////////////////////////////////////////////////////////////////
  // int32_t ReceiveFrequency()
  // Get sampling frequency of the last received payload.
  //
  // Return value:
  //   non-negative the sampling frequency in Hertz.
  //   -1 if an error has occurred.
  //
  virtual int32_t ReceiveFrequency() const = 0;

  ///////////////////////////////////////////////////////////////////////////
  // int32_t PlayoutFrequency()
  // Get sampling frequency of audio played out.
  //
  // Return value:
  //   the sampling frequency in Hertz.
  //
  virtual int32_t PlayoutFrequency() const = 0;

  // Replace any existing decoders with the given payload type -> decoder map.
  virtual void SetReceiveCodecs(
      const std::map<int, SdpAudioFormat>& codecs) = 0;

  // Registers a decoder for the given payload type. Returns true iff
  // successful.
  virtual bool RegisterReceiveCodec(int rtp_payload_type,
                                    const SdpAudioFormat& audio_format) = 0;

  // Registers an external decoder. The name is only used to provide information
  // back to the caller about the decoder. Hence, the name is arbitrary, and may
  // be empty.
  virtual int RegisterExternalReceiveCodec(int rtp_payload_type,
                                           AudioDecoder* external_decoder,
                                           int sample_rate_hz,
                                           int num_channels,
                                           const std::string& name) = 0;

  ///////////////////////////////////////////////////////////////////////////
  // int32_t UnregisterReceiveCodec()
  // Unregister the codec currently registered with a specific payload type
  // from the list of possible receive codecs.
  //
  // Input:
  //   -payload_type        : The number representing the payload type to
  //                         unregister.
  //
  // Output:
  //   -1 if fails to unregister.
  //    0 if the given codec is successfully unregistered.
  //
  virtual int UnregisterReceiveCodec(uint8_t payload_type) = 0;

  ///////////////////////////////////////////////////////////////////////////
  // int32_t ReceiveCodec()
  // Get the codec associated with last received payload.
  //
  // Output:
  //   -curr_receive_codec : parameters of the codec associated with the last
  //                         received payload, c.f. common_types.h for
  //                         the definition of CodecInst.
  //
  // Return value:
  //   -1 if failed to retrieve the codec,
  //    0 if the codec is successfully retrieved.
  //
  virtual int32_t ReceiveCodec(CodecInst* curr_receive_codec) const = 0;

  ///////////////////////////////////////////////////////////////////////////
  // absl::optional<SdpAudioFormat> ReceiveFormat()
  // Get the format associated with last received payload.
  //
  // Return value:
  //    An SdpAudioFormat describing the format associated with the last
  //    received payload.
  //    An empty Optional if no payload has yet been received.
  //
  virtual absl::optional<SdpAudioFormat> ReceiveFormat() const = 0;

  ///////////////////////////////////////////////////////////////////////////
  // int32_t IncomingPacket()
  // Call this function to insert a parsed RTP packet into ACM.
  //
  // Inputs:
  //   -incoming_payload   : received payload.
  //   -payload_len_bytes  : the length of payload in bytes.
  //   -rtp_info           : the relevant information retrieved from RTP
  //                         header.
  //
  // Return value:
  //   -1 if failed to push in the payload
  //    0 if payload is successfully pushed in.
  //
  virtual int32_t IncomingPacket(const uint8_t* incoming_payload,
                                 const size_t payload_len_bytes,
                                 const WebRtcRTPHeader& rtp_info) = 0;

  ///////////////////////////////////////////////////////////////////////////
  // int SetMinimumPlayoutDelay()
  // Set a minimum for the playout delay, used for lip-sync. NetEq maintains
  // such a delay unless channel condition yields to a higher delay.
  //
  // Input:
  //   -time_ms            : minimum delay in milliseconds.
  //
  // Return value:
  //   -1 if failed to set the delay,
  //    0 if the minimum delay is set.
  //
  virtual int SetMinimumPlayoutDelay(int time_ms) = 0;

  ///////////////////////////////////////////////////////////////////////////
  // int SetMaximumPlayoutDelay()
  // Set a maximum for the playout delay
  //
  // Input:
  //   -time_ms            : maximum delay in milliseconds.
  //
  // Return value:
  //   -1 if failed to set the delay,
  //    0 if the maximum delay is set.
  //
  virtual int SetMaximumPlayoutDelay(int time_ms) = 0;

  ///////////////////////////////////////////////////////////////////////////
  // int32_t PlayoutTimestamp()
  // The send timestamp of an RTP packet is associated with the decoded
  // audio of the packet in question. This function returns the timestamp of
  // the latest audio obtained by calling PlayoutData10ms(), or empty if no
  // valid timestamp is available.
  //
  virtual absl::optional<uint32_t> PlayoutTimestamp() = 0;

  ///////////////////////////////////////////////////////////////////////////
  // int FilteredCurrentDelayMs()
  // Returns the current total delay from NetEq (packet buffer and sync buffer)
  // in ms, with smoothing applied to even out short-time fluctuations due to
  // jitter. The packet buffer part of the delay is not updated during DTX/CNG
  // periods.
  //
  virtual int FilteredCurrentDelayMs() const = 0;

  ///////////////////////////////////////////////////////////////////////////
  // int FilteredCurrentDelayMs()
  // Returns the current target delay for NetEq in ms.
  //
  virtual int TargetDelayMs() const = 0;

  ///////////////////////////////////////////////////////////////////////////
  // int32_t PlayoutData10Ms(
  // Get 10 milliseconds of raw audio data for playout, at the given sampling
  // frequency. ACM will perform a resampling if required.
  //
  // Input:
  //   -desired_freq_hz    : the desired sampling frequency, in Hertz, of the
  //                         output audio. If set to -1, the function returns
  //                         the audio at the current sampling frequency.
  //
  // Output:
  //   -audio_frame        : output audio frame which contains raw audio data
  //                         and other relevant parameters.
  //   -muted              : if true, the sample data in audio_frame is not
  //                         populated, and must be interpreted as all zero.
  //
  // Return value:
  //   -1 if the function fails,
  //    0 if the function succeeds.
  //
  virtual int32_t PlayoutData10Ms(int32_t desired_freq_hz,
                                  AudioFrame* audio_frame,
                                  bool* muted) = 0;

  ///////////////////////////////////////////////////////////////////////////
  //   Codec specific
  //

  ///////////////////////////////////////////////////////////////////////////
  // int SetOpusMaxPlaybackRate()
  // If current send codec is Opus, informs it about maximum playback rate the
  // receiver will render. Opus can use this information to optimize the bit
  // rate and increase the computation efficiency.
  //
  // Input:
  //   -frequency_hz            : maximum playback rate in Hz.
  //
  // Return value:
  //   -1 if current send codec is not Opus or
  //      error occurred in setting the maximum playback rate,
  //    0 if maximum bandwidth is set successfully.
  //
  virtual int SetOpusMaxPlaybackRate(int frequency_hz) = 0;

  ///////////////////////////////////////////////////////////////////////////
  // EnableOpusDtx()
  // Enable the DTX, if current send codec is Opus.
  //
  // Return value:
  //   -1 if current send codec is not Opus or error occurred in enabling the
  //      Opus DTX.
  //    0 if Opus DTX is enabled successfully.
  //
  virtual int EnableOpusDtx() = 0;

  ///////////////////////////////////////////////////////////////////////////
  // int DisableOpusDtx()
  // If current send codec is Opus, disables its internal DTX.
  //
  // Return value:
  //   -1 if current send codec is not Opus or error occurred in disabling DTX.
  //    0 if Opus DTX is disabled successfully.
  //
  virtual int DisableOpusDtx() = 0;

  ///////////////////////////////////////////////////////////////////////////
  //   statistics
  //

  ///////////////////////////////////////////////////////////////////////////
  // int32_t  GetNetworkStatistics()
  // Get network statistics. Note that the internal statistics of NetEq are
  // reset by this call.
  //
  // Input:
  //   -network_statistics : a structure that contains network statistics.
  //
  // Return value:
  //   -1 if failed to set the network statistics,
  //    0 if statistics are set successfully.
  //
  virtual int32_t GetNetworkStatistics(
      NetworkStatistics* network_statistics) = 0;

  //
  // Enable NACK and set the maximum size of the NACK list. If NACK is already
  // enable then the maximum NACK list size is modified accordingly.
  //
  // If the sequence number of last received packet is N, the sequence numbers
  // of NACK list are in the range of [N - |max_nack_list_size|, N).
  //
  // |max_nack_list_size| should be positive (none zero) and less than or
  // equal to |Nack::kNackListSizeLimit|. Otherwise, No change is applied and -1
  // is returned. 0 is returned at success.
  //
  virtual int EnableNack(size_t max_nack_list_size) = 0;

  // Disable NACK.
  virtual void DisableNack() = 0;

  //
  // Get a list of packets to be retransmitted. |round_trip_time_ms| is an
  // estimate of the round-trip-time (in milliseconds). Missing packets which
  // will be playout in a shorter time than the round-trip-time (with respect
  // to the time this API is called) will not be included in the list.
  //
  // Negative |round_trip_time_ms| results is an error message and empty list
  // is returned.
  //
  virtual std::vector<uint16_t> GetNackList(
      int64_t round_trip_time_ms) const = 0;

  virtual void GetDecodingCallStatistics(
      AudioDecodingCallStats* call_stats) const = 0;

  virtual ANAStats GetANAStats() const = 0;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_CODING_INCLUDE_AUDIO_CODING_MODULE_H_
