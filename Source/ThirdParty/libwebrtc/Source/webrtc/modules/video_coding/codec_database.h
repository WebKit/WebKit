/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_CODEC_DATABASE_H_
#define WEBRTC_MODULES_VIDEO_CODING_CODEC_DATABASE_H_

#include <map>
#include <memory>

#include "webrtc/modules/video_coding/include/video_codec_interface.h"
#include "webrtc/modules/video_coding/include/video_coding.h"
#include "webrtc/modules/video_coding/generic_decoder.h"
#include "webrtc/modules/video_coding/generic_encoder.h"
#include "webrtc/typedefs.h"

namespace webrtc {

struct VCMDecoderMapItem {
 public:
  VCMDecoderMapItem(VideoCodec* settings,
                    int number_of_cores,
                    bool require_key_frame);

  std::unique_ptr<VideoCodec> settings;
  int number_of_cores;
  bool require_key_frame;
};

struct VCMExtDecoderMapItem {
 public:
  VCMExtDecoderMapItem(VideoDecoder* external_decoder_instance,
                       uint8_t payload_type);

  uint8_t payload_type;
  VideoDecoder* external_decoder_instance;
};

class VCMCodecDataBase {
 public:
  explicit VCMCodecDataBase(VCMEncodedFrameCallback* encoded_frame_callback);
  ~VCMCodecDataBase();

  // Sender Side
  // Returns the default settings for the codec with type |codec_type|.
  static void Codec(VideoCodecType codec_type, VideoCodec* settings);

  // Sets the sender side codec and initiates the desired codec given the
  // VideoCodec struct.
  // Returns true if the codec was successfully registered, false otherwise.
  bool SetSendCodec(const VideoCodec* send_codec,
                    int number_of_cores,
                    size_t max_payload_size);

  // Gets the current send codec. Relevant for internal codecs only.
  // Returns true if there is a send codec, false otherwise.
  bool SendCodec(VideoCodec* current_send_codec) const;

  // Gets current send side codec type. Relevant for internal codecs only.
  // Returns kVideoCodecUnknown if there is no send codec.
  VideoCodecType SendCodec() const;

  // Registers and initializes an external encoder object.
  // |internal_source| should be set to true if the codec has an internal
  // video source and doesn't need the user to provide it with frames via
  // the Encode() method.
  void RegisterExternalEncoder(VideoEncoder* external_encoder,
                               uint8_t payload_type,
                               bool internal_source);

  // Deregisters an external encoder. Returns true if the encoder was
  // found and deregistered, false otherwise. |was_send_codec| is set to true
  // if the external encoder was the send codec before being deregistered.
  bool DeregisterExternalEncoder(uint8_t payload_type, bool* was_send_codec);

  VCMGenericEncoder* GetEncoder();

  bool SetPeriodicKeyFrames(bool enable);

  // Deregisters an external decoder object specified by |payload_type|.
  bool DeregisterExternalDecoder(uint8_t payload_type);

  // Registers an external decoder object to the payload type |payload_type|.
  void RegisterExternalDecoder(VideoDecoder* external_decoder,
                               uint8_t payload_type);

  bool DecoderRegistered() const;

  bool RegisterReceiveCodec(const VideoCodec* receive_codec,
                            int number_of_cores,
                            bool require_key_frame);

  bool DeregisterReceiveCodec(uint8_t payload_type);

  // Get current receive side codec. Relevant for internal codecs only.
  bool ReceiveCodec(VideoCodec* current_receive_codec) const;

  // Get current receive side codec type. Relevant for internal codecs only.
  VideoCodecType ReceiveCodec() const;

  // Returns a decoder specified by |payload_type|. The decoded frame callback
  // of the encoder is set to |decoded_frame_callback|. If no such decoder
  // already exists an instance will be created and initialized.
  // NULL is returned if no encoder with the specified payload type was found
  // and the function failed to create one.
  VCMGenericDecoder* GetDecoder(
      const VCMEncodedFrame& frame,
      VCMDecodedFrameCallback* decoded_frame_callback);

  // Deletes the memory of the decoder instance |decoder|. Used to delete
  // deep copies returned by CreateDecoderCopy().
  void ReleaseDecoder(VCMGenericDecoder* decoder) const;

  // Returns true if the currently active decoder prefer to decode frames late.
  // That means that frames must be decoded near the render times stamp.
  bool PrefersLateDecoding() const;

  bool MatchesCurrentResolution(int width, int height) const;

 private:
  typedef std::map<uint8_t, VCMDecoderMapItem*> DecoderMap;
  typedef std::map<uint8_t, VCMExtDecoderMapItem*> ExternalDecoderMap;

  VCMGenericDecoder* CreateAndInitDecoder(const VCMEncodedFrame& frame,
                                          VideoCodec* new_codec) const;

  // Determines whether a new codec has to be created or not.
  // Checks every setting apart from maxFramerate and startBitrate.
  bool RequiresEncoderReset(const VideoCodec& send_codec);

  void DeleteEncoder();

  // Create an internal Decoder given a codec type
  VCMGenericDecoder* CreateDecoder(VideoCodecType type) const;

  const VCMDecoderMapItem* FindDecoderItem(uint8_t payload_type) const;

  const VCMExtDecoderMapItem* FindExternalDecoderItem(
      uint8_t payload_type) const;

  int number_of_cores_;
  size_t max_payload_size_;
  bool periodic_key_frames_;
  bool pending_encoder_reset_;
  VideoCodec send_codec_;
  VideoCodec receive_codec_;
  uint8_t encoder_payload_type_;
  VideoEncoder* external_encoder_;
  bool internal_source_;
  VCMEncodedFrameCallback* const encoded_frame_callback_;
  std::unique_ptr<VCMGenericEncoder> ptr_encoder_;
  VCMGenericDecoder* ptr_decoder_;
  DecoderMap dec_map_;
  ExternalDecoderMap dec_external_map_;
};  // VCMCodecDataBase

}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_CODEC_DATABASE_H_
