/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *  Copyright (c) 2023 Apple Inc. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#if WEBRTC_WEBKIT_BUILD

// Simple Encoder
// ==============
//
// This is an example of a simple encoder loop. It takes an input file in
// YV12 format, passes it through the encoder, and writes the compressed
// frames to disk in IVF format. Other decoder examples build upon this
// one.
//
// The details of the IVF format have been elided from this example for
// simplicity of presentation, as IVF files will not generally be used by
// your application. In general, an IVF file consists of a file header,
// followed by a variable number of frames. Each frame consists of a frame
// header followed by a variable length payload. The length of the payload
// is specified in the first four bytes of the frame header. The payload is
// the raw compressed data.
//
// Standard Includes
// -----------------
// For encoders, you only have to include `vpx_encoder.h` and then any
// header files for the specific codecs you use. In this case, we're using
// vp8.
//
// Getting The Default Configuration
// ---------------------------------
// Encoders have the notion of "usage profiles." For example, an encoder
// may want to publish default configurations for both a video
// conferencing application and a best quality offline encoder. These
// obviously have very different default settings. Consult the
// documentation for your codec to see if it provides any default
// configurations. All codecs provide a default configuration, number 0,
// which is valid for material in the vacinity of QCIF/QVGA.
//
// Updating The Configuration
// ---------------------------------
// Almost all applications will want to update the default configuration
// with settings specific to their usage. Here we set the width and height
// of the video file to that specified on the command line. We also scale
// the default bitrate based on the ratio between the default resolution
// and the resolution specified on the command line.
//
// Initializing The Codec
// ----------------------
// The encoder is initialized by the following code.
//
// Encoding A Frame
// ----------------
// The frame is read as a continuous block (size width * height * 3 / 2)
// from the input file. If a frame was read (the input file has not hit
// EOF) then the frame is passed to the encoder. Otherwise, a NULL
// is passed, indicating the End-Of-Stream condition to the encoder. The
// `frame_cnt` is reused as the presentation time stamp (PTS) and each
// frame is shown for one frame-time in duration. The flags parameter is
// unused in this example. The deadline is set to VPX_DL_REALTIME to
// make the example run as quickly as possible.

// Forced Keyframes
// ----------------
// Keyframes can be forced by setting the VPX_EFLAG_FORCE_KF bit of the
// flags passed to `vpx_codec_control()`. In this example, we force a
// keyframe every <keyframe-interval> frames. Note, the output stream can
// contain additional keyframes beyond those that have been forced using the
// VPX_EFLAG_FORCE_KF flag because of automatic keyframe placement by the
// encoder.
//
// Processing The Encoded Data
// ---------------------------
// Each packet of type `VPX_CODEC_CX_FRAME_PKT` contains the encoded data
// for this frame. We write a IVF frame header, followed by the raw data.
//
// Cleanup
// -------
// The `vpx_codec_destroy` call frees any memory allocated by the codec.
//
// Error Handling
// --------------
// This example does not special case any error return codes. If there was
// an error, a descriptive message is printed and the program exits. With
// few exeptions, vpx_codec functions return an enumerated error status,
// with the value `0` indicating success.
//
// Error Resiliency Features
// -------------------------
// Error resiliency is controlled by the g_error_resilient member of the
// configuration structure. Use the `decode_with_drops` example to decode with
// frames 5-10 dropped. Compare the output for a file encoded with this example
// versus one encoded with the `simple_encoder` example.

#include <span>

#include "test/fuzzers/fuzz_data_helper.h"
#include "tools_common.h"
#include "vpx/vpx_encoder.h"

#define VPX_ENCODER_NAME(name) VPX_ENCODER_NAME_(name)
#define VPX_ENCODER_NAME_(name) #name

// Taken from libvpx/source/libvpx/tools_common.c.
static bool vpx_img_read(vpx_image_t *img, webrtc::test::FuzzDataHelper& fuzz_input) {
  for (int plane = 0; plane < 3; ++plane) {
    if (plane == VPX_PLANE_V && img->fmt == VPX_IMG_FMT_NV12)
      continue;
    unsigned char *buf = img->planes[plane];
    if (!buf && img->fmt == VPX_IMG_FMT_NONE)
      continue;
    const int stride = img->stride[plane];
    const int w = vpx_img_plane_width(img, plane) *
                  ((img->fmt & VPX_IMG_FMT_HIGHBITDEPTH) ? 2 : 1);
    const int h = vpx_img_plane_height(img, plane);

    for (int y = 0; y < h; ++y) {
      auto byte_array = fuzz_input.ReadByteArray((size_t)w);
      if (byte_array.empty())
        return false;
      memcpy(buf, byte_array.data(), byte_array.size());
      buf += stride;
    }
  }

  return true;
}

static std::span<uint8_t> encode_frame(vpx_codec_ctx_t *codec, vpx_image_t *img,
                                       int frame_index, int flags, unsigned long deadline) {
  bool got_pkts = false;
  vpx_codec_iter_t iter = NULL;
  const vpx_codec_cx_pkt_t *pkt = NULL;
  const vpx_codec_err_t res =
      vpx_codec_encode(codec, img, frame_index, 1, flags, deadline);
  if (res != VPX_CODEC_OK)
    return { };

  std::span<uint8_t> buffer { (uint8_t *)malloc(0), 0 };
  size_t buffer_offset = 0;

  while ((pkt = vpx_codec_get_cx_data(codec, &iter)) != NULL) {
    got_pkts = true;
    if (pkt->kind == VPX_CODEC_CX_FRAME_PKT) {
      size_t buffer_size = buffer.size() + pkt->data.frame.sz;
      buffer = { (uint8_t *)reallocf(buffer.data(), buffer_size), buffer_size };
      if (buffer.empty())
        return { };
      memcpy(buffer.subspan(buffer_offset).data(), pkt->data.frame.buf, pkt->data.frame.sz);
      buffer_offset = buffer.size();
    }
  }

  if (!got_pkts) {
    free(buffer.data());
    return { };
  }

  return buffer;
}

extern "C" void usage_exit(void) { exit(EXIT_FAILURE); }

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  webrtc::test::FuzzDataHelper fuzz_input(webrtc::MakeArrayView(data, size));

  vpx_codec_ctx_t codec;
  vpx_codec_enc_cfg_t cfg;
  int frame_count = 0;
  vpx_codec_err_t res;

  const VpxInterface *encoder = get_vpx_encoder_by_name(VPX_ENCODER_NAME(ENCODER));
  const int fps = abs(fuzz_input.ReadOrDefaultValue<int>(30)) || 30;
  const unsigned bitrate = fuzz_input.ReadOrDefaultValue<unsigned>(200) || 200;
  const int keyframe_interval = abs(fuzz_input.ReadOrDefaultValue<int>(0));
  int frame_width = abs(fuzz_input.ReadOrDefaultValue<int>(320)) || 320;
  frame_width += (frame_width % 2);
  int frame_height = abs(fuzz_input.ReadOrDefaultValue<int>(240)) || 240;
  frame_height += (frame_height % 2);
  const uint32_t error_resilient = fuzz_input.ReadOrDefaultValue<uint32_t>(0);

  // See vpx_encoder.h.
  const unsigned long kVpxEncoderDeadline[] = {
    VPX_DL_BEST_QUALITY,
    VPX_DL_REALTIME,
    VPX_DL_GOOD_QUALITY,
  };
  unsigned long deadline = fuzz_input.SelectOneOf(kVpxEncoderDeadline);

  enum vpx_img_fmt kVpxImageFormats[] = {
      VPX_IMG_FMT_NONE,
      VPX_IMG_FMT_YV12,
      VPX_IMG_FMT_I420,
      VPX_IMG_FMT_I422,
      VPX_IMG_FMT_I444,
      VPX_IMG_FMT_I440,
      VPX_IMG_FMT_NV12,
      VPX_IMG_FMT_I42016,
      VPX_IMG_FMT_I42216,
      VPX_IMG_FMT_I44416,
      VPX_IMG_FMT_I44016,
  };
  vpx_img_fmt_t vpx_image_format = fuzz_input.SelectOneOf(kVpxImageFormats);

  auto raw = std::unique_ptr<vpx_image_t, void(*)(vpx_image_t*)>(vpx_img_alloc(NULL, vpx_image_format, frame_width, frame_height, 1), vpx_img_free);
  if (!raw)
      return 0;

  res = vpx_codec_enc_config_default(encoder->codec_interface(), &cfg, 0);
  if (res)
    die_codec(&codec, "Failed to get default codec config.");

  cfg.g_w = frame_width;
  cfg.g_h = frame_height;
  cfg.g_threads = 1U << (fuzz_input.ReadOrDefaultValue<unsigned>(0) % 4);
  cfg.g_timebase.num = 1;
  cfg.g_timebase.den = fps;
  cfg.rc_target_bitrate = bitrate;
  cfg.g_error_resilient = (vpx_codec_er_flags_t)error_resilient;

  if (vpx_codec_enc_init(&codec, encoder->codec_interface(), &cfg, raw->bit_depth == 16 ? VPX_CODEC_USE_HIGHBITDEPTH : 0))
    die("Failed to initialize encoder");

  // TODO: Set more options here.
  // if (vpx_codec_control_(&codec, VP9E_SET_LOSSLESS, 1))
  //   die_codec(&codec, "Failed to use lossless mode");

  // Encode frames.
  while (vpx_img_read(raw.get(), fuzz_input)) {
    int flags = 0;
    if (keyframe_interval > 0 && frame_count % keyframe_interval == 0)
        flags |= VPX_EFLAG_FORCE_KF;
    auto encoded_frame = encode_frame(&codec, raw.get(), frame_count++, flags, deadline);
    if (encoded_frame.empty())
      break;
    free(encoded_frame.data());
  }

  // Flush encoder.
  while (true) {
    auto encoded_frame = encode_frame(&codec, NULL, -1, 0, deadline);
    if (encoded_frame.empty())
      break;
    free(encoded_frame.data());
  }

  if (vpx_codec_destroy(&codec))
    die_codec(&codec, "Failed to destroy codec.");

  return 0;
}

#endif // WEBRTC_WEBKIT_BUILD
