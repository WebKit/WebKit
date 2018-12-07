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

#include <stdint.h>

#include "absl/types/optional.h"
#include "api/video/hdr_metadata.h"

namespace webrtc {

// This class represents color information as specified in T-REC H.273,
// available from https://www.itu.int/rec/T-REC-H.273.
//
// WebRTC's supported codecs:
// - VP9 supports color profiles, see VP9 Bitstream & Decoding Process
// Specification Version 0.6 Section 7.2.2 "Color config semantics" available
// from https://www.webmproject.org.
// - VP8 only supports BT.601, see
// https://tools.ietf.org/html/rfc6386#section-9.2
// - H264 uses the exact same representation as T-REC H.273. See T-REC-H.264
// E.2.1, "VUI parameters semantics", available from
// https://www.itu.int/rec/T-REC-H.264.

class ColorSpace {
 public:
  enum class PrimaryID : uint8_t {
    // The indices are equal to the values specified in T-REC H.273 Table 2.
    kInvalid = 0,
    kBT709 = 1,
    kUNSPECIFIED = 2,
    kBT470M = 4,
    kBT470BG = 5,
    kSMPTE170M = 6,  // Identical to BT601
    kSMPTE240M = 7,
    kFILM = 8,
    kBT2020 = 9,
    kSMPTEST428 = 10,
    kSMPTEST431 = 11,
    kSMPTEST432 = 12,
    kJEDECP22 = 22,  // Identical to EBU3213-E
    // When adding/removing entries here, please make sure to do the
    // corresponding change to kPrimaryIds.
  };

  enum class TransferID : uint8_t {
    // The indices are equal to the values specified in T-REC H.273 Table 3.
    kInvalid = 0,
    kBT709 = 1,
    kUNSPECIFIED = 2,
    kGAMMA22 = 4,
    kGAMMA28 = 5,
    kSMPTE170M = 6,
    kSMPTE240M = 7,
    kLINEAR = 8,
    kLOG = 9,
    kLOG_SQRT = 10,
    kIEC61966_2_4 = 11,
    kBT1361_ECG = 12,
    kIEC61966_2_1 = 13,
    kBT2020_10 = 14,
    kBT2020_12 = 15,
    kSMPTEST2084 = 16,
    kSMPTEST428 = 17,
    kARIB_STD_B67 = 18,
    // When adding/removing entries here, please make sure to do the
    // corresponding change to kTransferIds.
  };

  enum class MatrixID : uint8_t {
    // The indices are equal to the values specified in T-REC H.273 Table 4.
    kRGB = 0,
    kBT709 = 1,
    kUNSPECIFIED = 2,
    kFCC = 4,
    kBT470BG = 5,
    kSMPTE170M = 6,
    kSMPTE240M = 7,
    kYCOCG = 8,
    kBT2020_NCL = 9,
    kBT2020_CL = 10,
    kSMPTE2085 = 11,
    kCDNCLS = 12,
    kCDCLS = 13,
    kBT2100_ICTCP = 14,
    kInvalid = 63,
    // When adding/removing entries here, please make sure to do the
    // corresponding change to kMatrixIds.
  };

  enum class RangeID {
    // The indices are equal to the values specified at
    // https://www.webmproject.org/docs/container/#colour for the element Range.
    kInvalid = 0,
    // Limited Rec. 709 color range with RGB values ranging from 16 to 235.
    kLimited = 1,
    // Full RGB color range with RGB valees from 0 to 255.
    kFull = 2,
    // Range is defined by MatrixCoefficients/TransferCharacteristics.
    kDerived = 3,
    // When adding/removing entries here, please make sure to do the
    // corresponding change to kRangeIds.
  };

  ColorSpace();
  ColorSpace(const ColorSpace& other);
  ColorSpace(ColorSpace&& other);
  ColorSpace& operator=(const ColorSpace& other);
  ColorSpace(PrimaryID primaries,
             TransferID transfer,
             MatrixID matrix,
             RangeID full_range);
  ColorSpace(PrimaryID primaries,
             TransferID transfer,
             MatrixID matrix,
             RangeID range,
             const HdrMetadata* hdr_metadata);
  bool operator==(const ColorSpace& other) const {
    return primaries_ == other.primaries() && transfer_ == other.transfer() &&
           matrix_ == other.matrix() && range_ == other.range() &&
           ((hdr_metadata_.has_value() && other.hdr_metadata() &&
             *hdr_metadata_ == *other.hdr_metadata()) ||
            (!hdr_metadata_.has_value() && other.hdr_metadata() == nullptr));
  }

  PrimaryID primaries() const;
  TransferID transfer() const;
  MatrixID matrix() const;
  RangeID range() const;
  const HdrMetadata* hdr_metadata() const;

  bool set_primaries_from_uint8(uint8_t enum_value);
  bool set_transfer_from_uint8(uint8_t enum_value);
  bool set_matrix_from_uint8(uint8_t enum_value);
  bool set_range_from_uint8(uint8_t enum_value);
  void set_hdr_metadata(const HdrMetadata* hdr_metadata);

 private:
  PrimaryID primaries_ = PrimaryID::kInvalid;
  TransferID transfer_ = TransferID::kInvalid;
  MatrixID matrix_ = MatrixID::kInvalid;
  RangeID range_ = RangeID::kInvalid;
  absl::optional<HdrMetadata> hdr_metadata_;
};

}  // namespace webrtc

#endif  // API_VIDEO_COLOR_SPACE_H_
