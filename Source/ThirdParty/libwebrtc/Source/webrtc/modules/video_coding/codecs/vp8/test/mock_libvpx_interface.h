/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_CODECS_VP8_TEST_MOCK_LIBVPX_INTERFACE_H_
#define MODULES_VIDEO_CODING_CODECS_VP8_TEST_MOCK_LIBVPX_INTERFACE_H_

#include "modules/video_coding/codecs/vp8/libvpx_interface.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

class MockLibvpxVp8Interface : public LibvpxInterface {
 public:
  MOCK_CONST_METHOD5(img_alloc,
                     vpx_image_t*(vpx_image_t*,
                                  vpx_img_fmt_t,
                                  unsigned int,
                                  unsigned int,
                                  unsigned int));
  MOCK_CONST_METHOD6(img_wrap,
                     vpx_image_t*(vpx_image_t*,
                                  vpx_img_fmt_t,
                                  unsigned int,
                                  unsigned int,
                                  unsigned int,
                                  unsigned char*));
  MOCK_CONST_METHOD1(img_free, void(vpx_image_t* img));
  MOCK_CONST_METHOD2(codec_enc_config_set,
                     vpx_codec_err_t(vpx_codec_ctx_t*,
                                     const vpx_codec_enc_cfg_t*));
  MOCK_CONST_METHOD3(codec_enc_config_default,
                     vpx_codec_err_t(vpx_codec_iface_t*,
                                     vpx_codec_enc_cfg_t*,
                                     unsigned int));
  MOCK_CONST_METHOD4(codec_enc_init,
                     vpx_codec_err_t(vpx_codec_ctx_t*,
                                     vpx_codec_iface_t*,
                                     const vpx_codec_enc_cfg_t*,
                                     vpx_codec_flags_t));
  MOCK_CONST_METHOD6(codec_enc_init_multi,
                     vpx_codec_err_t(vpx_codec_ctx_t*,
                                     vpx_codec_iface_t*,
                                     vpx_codec_enc_cfg_t*,
                                     int,
                                     vpx_codec_flags_t,
                                     vpx_rational_t*));
  MOCK_CONST_METHOD1(codec_destroy, vpx_codec_err_t(vpx_codec_ctx_t*));
  MOCK_CONST_METHOD3(codec_control,
                     vpx_codec_err_t(vpx_codec_ctx_t*,
                                     vp8e_enc_control_id,
                                     uint32_t));
  MOCK_CONST_METHOD3(codec_control,
                     vpx_codec_err_t(vpx_codec_ctx_t*,
                                     vp8e_enc_control_id,
                                     int));
  MOCK_CONST_METHOD3(codec_control,
                     vpx_codec_err_t(vpx_codec_ctx_t*,
                                     vp8e_enc_control_id,
                                     int*));
  MOCK_CONST_METHOD3(codec_control,
                     vpx_codec_err_t(vpx_codec_ctx_t*,
                                     vp8e_enc_control_id,
                                     vpx_roi_map*));
  MOCK_CONST_METHOD3(codec_control,
                     vpx_codec_err_t(vpx_codec_ctx_t*,
                                     vp8e_enc_control_id,
                                     vpx_active_map*));
  MOCK_CONST_METHOD3(codec_control,
                     vpx_codec_err_t(vpx_codec_ctx_t*,
                                     vp8e_enc_control_id,
                                     vpx_scaling_mode*));
  MOCK_CONST_METHOD6(codec_encode,
                     vpx_codec_err_t(vpx_codec_ctx_t*,
                                     const vpx_image_t*,
                                     vpx_codec_pts_t,
                                     uint64_t,
                                     vpx_enc_frame_flags_t,
                                     uint64_t));
  MOCK_CONST_METHOD2(codec_get_cx_data,
                     const vpx_codec_cx_pkt_t*(vpx_codec_ctx_t*,
                                               vpx_codec_iter_t*));
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_CODECS_VP8_TEST_MOCK_LIBVPX_INTERFACE_H_
