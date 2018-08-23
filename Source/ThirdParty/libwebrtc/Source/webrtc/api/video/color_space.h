/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_COLOR_SPACE_H_
#define API_VIDEO_COLOR_SPACE_H_

namespace webrtc {

// Used to represent a color space for the purpose of color conversion. This
// class only represents color information that can be transferred through the
// bitstream of WebRTC's internal supported codecs:
// - VP9 supports color profiles, see VP9 Bitstream & Decoding Process
// Specification Version 0.6 Section 7.2.2 "Color config semantics" available
// from https://www.webmproject.org.
// TODO(emircan): Extract these values from decode and add to the existing ones.
// - VP8 only supports BT.601, see
// https://tools.ietf.org/html/rfc6386#section-9.2
// - H264 supports different color primaries, transfer characteristics, matrix
// coefficients and range. See T-REC-H.264 E.2.1, "VUI parameters semantics",
// available from https://www.itu.int/rec/T-REC-H.264.
class ColorSpace {
 public:
  enum class PrimaryID {
    kInvalid,
    kBT709,
    kBT470M,
    kBT470BG,
    kSMPTE170M,  // Identical to BT601
    kSMPTE240M,
    kFILM,
    kBT2020,
    kSMPTEST428,
    kSMPTEST431,
    kSMPTEST432,
    kJEDECP22,
  };

  enum class TransferID {
    kInvalid,
    kBT709,
    kGAMMA22,
    kGAMMA28,
    kSMPTE170M,
    kSMPTE240M,
    kLINEAR,
    kLOG,
    kLOG_SQRT,
    kIEC61966_2_4,
    kBT1361_ECG,
    kIEC61966_2_1,
    kBT2020_10,
    kBT2020_12,
    kSMPTEST2084,
    kSMPTEST428,
    kARIB_STD_B67,
  };

  enum class MatrixID {
    kInvalid,
    kRGB,
    kBT709,
    kFCC,
    kBT470BG,
    kSMPTE170M,
    kSMPTE240M,
    kYCOCG,
    kBT2020_NCL,
    kBT2020_CL,
    kSMPTE2085,
  };

  enum class RangeID {
    kInvalid,
    // Limited Rec. 709 color range with RGB values ranging from 16 to 235.
    kLimited,
    // Full RGB color range with RGB valees from 0 to 255.
    kFull,
  };

  ColorSpace();
  ColorSpace(PrimaryID primaries,
             TransferID transfer,
             MatrixID matrix,
             RangeID full_range);

  PrimaryID primaries() const;
  TransferID transfer() const;
  MatrixID matrix() const;
  RangeID range() const;

 private:
  PrimaryID primaries_ = PrimaryID::kInvalid;
  TransferID transfer_ = TransferID::kInvalid;
  MatrixID matrix_ = MatrixID::kInvalid;
  RangeID range_ = RangeID::kInvalid;
};

}  // namespace webrtc

#endif  // API_VIDEO_COLOR_SPACE_H_
