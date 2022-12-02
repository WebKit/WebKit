/*
 * Copyright (c) 2022, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#ifndef AOM_AV1_RATECTRL_QMODE_INTERFACE_H_
#define AOM_AV1_RATECTRL_QMODE_INTERFACE_H_

#include <array>
#include <string>
#include <vector>

#include "aom/aom_codec.h"
#include "av1/encoder/firstpass.h"

namespace aom {

constexpr int kBlockRefCount = 2;

struct MotionVector {
  int row;          // subpel row
  int col;          // subpel col
  int subpel_bits;  // number of fractional bits used by row/col
};

struct RateControlParam {
  // Range of allowed GOP sizes (number of displayed frames).
  int max_gop_show_frame_count;
  int min_gop_show_frame_count;
  // Number of reference frame buffers, i.e., size of the DPB.
  int ref_frame_table_size;
  // Maximum number of references a single frame may use.
  int max_ref_frames;
  // Maximum pyramid depth. e.g., 1 means only one ARF per GOP,
  // 2 would allow an additional level of intermediate ARFs.
  int max_depth;

  int base_q_index;

  int frame_width;
  int frame_height;
};

struct TplBlockStats {
  int height;  // pixel height
  int width;   // pixel width
  int row;     // pixel row of the top left corner
  int col;     // pixel col of the top lef corner
  int64_t intra_cost;
  int64_t inter_cost;
  std::array<MotionVector, kBlockRefCount> mv;
  std::array<int, kBlockRefCount> ref_frame_index;
};

// gop frame type used for facilitate setting up GopFrame
// TODO(angiebird): Define names for forward key frame and
// key frame with overlay
enum class GopFrameType {
  kRegularKey,     // High quality key frame without overlay
  kRegularLeaf,    // Regular leaf frame
  kRegularGolden,  // Regular golden frame
  kRegularArf,  // High quality arf with strong filtering followed by an overlay
                // later
  kOverlay,     // Overlay frame
  kIntermediateOverlay,  // Intermediate overlay frame
  kIntermediateArf,  // Good quality arf with weak or no filtering followed by a
                     // show_existing later
};

enum class EncodeRefMode {
  kRegular,
  kOverlay,
  kShowExisting,
};

enum class ReferenceName {
  kNoneFrame = -1,
  kIntraFrame = 0,
  kLastFrame = 1,
  kLast2Frame = 2,
  kLast3Frame = 3,
  kGoldenFrame = 4,
  kBwdrefFrame = 5,
  kAltref2Frame = 6,
  kAltrefFrame = 7,
};

struct Status {
  aom_codec_err_t code;
  std::string message;  // Empty if code == AOM_CODEC_OK.
  bool ok() const { return code == AOM_CODEC_OK; }
};

// A very simple imitation of absl::StatusOr, this is conceptually a union of a
// Status struct and an object of type T. It models an object that is either a
// usable object, or an error explaining why such an object is not present. A
// StatusOr<T> may never hold a status with a code of AOM_CODEC_OK.
template <typename T>
class StatusOr {
 public:
  StatusOr(const T &value) : value_(value) {}
  StatusOr(T &&value) : value_(std::move(value)) {}
  StatusOr(Status status) : status_(std::move(status)) {
    assert(status_.code != AOM_CODEC_OK);
  }

  const Status &status() const { return status_; }
  bool ok() const { return status().ok(); }

  // operator* returns the value; it should only be called after checking that
  // ok() returns true.
  const T &operator*() const & { return value_; }
  T &operator*() & { return value_; }
  const T &&operator*() const && { return value_; }
  T &&operator*() && { return std::move(value_); }

  // sor->field is equivalent to (*sor).field.
  const T *operator->() const & { return &value_; }
  T *operator->() & { return &value_; }

  // value() is equivalent to operator*, but asserts that ok() is true.
  const T &value() const & {
    assert(ok());
    return value_;
  }
  T &value() & {
    assert(ok());
    return value_;
  }
  const T &&value() const && {
    assert(ok());
    return value_;
  }
  T &&value() && {
    assert(ok());
    return std::move(value_);
  }

 private:
  T value_;  // This could be std::optional<T> if it were available.
  Status status_ = { AOM_CODEC_OK, "" };
};

struct ReferenceFrame {
  int index;  // Index of reference slot containing the reference frame
  ReferenceName name;
};

struct GopFrame {
  // basic info
  bool is_valid;
  int order_idx;    // Index in display order in a GOP
  int coding_idx;   // Index in coding order in a GOP
  int display_idx;  // The number of displayed frames preceding this frame in
                    // a GOP

  int global_order_idx;   // Index in display order in the whole video chunk
  int global_coding_idx;  // Index in coding order in the whole video chunk

  bool is_key_frame;     // If this is key frame, reset reference buffers are
                         // required
  bool is_arf_frame;     // Is this a forward frame, a frame with order_idx
                         // higher than the current display order
  bool is_show_frame;    // Is this frame a show frame after coding
  bool is_golden_frame;  // Is this a high quality frame

  GopFrameType update_type;  // This is a redundant field. It is only used for
                             // easy conversion in SW integration.

  // reference frame info
  EncodeRefMode encode_ref_mode;
  int colocated_ref_idx;  // colocated_ref_idx == -1 when encode_ref_mode ==
                          // EncodeRefMode::kRegular
  int update_ref_idx;     // The reference index that this frame should be
                          // updated to. update_ref_idx == -1 when this frame
                          // will not serve as a reference frame
  std::vector<ReferenceFrame>
      ref_frame_list;  // A list of available reference frames in priority order
                       // for the current to-be-coded frame. The list size
                       // should be less or equal to ref_frame_table_size. The
                       // reference frames with smaller indices are more likely
                       // to be a good reference frame. Therefore, they should
                       // be prioritized when the reference frame count is
                       // limited. For example, if we plan to use 3 reference
                       // frames, we should choose ref_frame_list[0],
                       // ref_frame_list[1] and ref_frame_list[2].
  int layer_depth;     // Layer depth in the GOP structure
  ReferenceFrame primary_ref_frame;  // We will use the primary reference frame
                                     // to update current frame's initial
                                     // probability model
};

struct GopStruct {
  int show_frame_count;
  int global_coding_idx_offset;
  int global_order_idx_offset;
  // TODO(jingning): This can be removed once the framework is up running.
  int display_tracker;  // Track the number of frames displayed proceeding a
                        // current coding frame.
  std::vector<GopFrame> gop_frame_list;
};

using GopStructList = std::vector<GopStruct>;

struct FrameEncodeParameters {
  int q_index;
  int rdmult;
};

struct FirstpassInfo {
  int num_mbs_16x16;  // Count of 16x16 unit blocks in each frame.
                      // FIRSTPASS_STATS's unit block size is 16x16
  std::vector<FIRSTPASS_STATS> stats_list;
};

// In general, the number of elements in RefFrameTable must always equal
// ref_frame_table_size (as specified in RateControlParam), but see
// GetGopEncodeInfo for the one exception.
using RefFrameTable = std::vector<GopFrame>;

struct GopEncodeInfo {
  std::vector<FrameEncodeParameters> param_list;
  RefFrameTable final_snapshot;  // RefFrameTable snapshot after coding this GOP
};

struct TplFrameStats {
  int min_block_size;
  int frame_width;
  int frame_height;
  std::vector<TplBlockStats> block_stats_list;
};

struct TplGopStats {
  std::vector<TplFrameStats> frame_stats_list;
};

class AV1RateControlQModeInterface {
 public:
  AV1RateControlQModeInterface();
  virtual ~AV1RateControlQModeInterface();

  virtual Status SetRcParam(const RateControlParam &rc_param) = 0;
  virtual StatusOr<GopStructList> DetermineGopInfo(
      const FirstpassInfo &firstpass_info) = 0;
  // Accept firstpass and TPL info from the encoder and return q index and
  // rdmult. This needs to be called with consecutive GOPs as returned by
  // DetermineGopInfo.
  // For the first GOP, a default-constructed RefFrameTable may be passed in as
  // ref_frame_table_snapshot_init; for subsequent GOPs, it should be the
  // final_snapshot returned on the previous call.
  virtual StatusOr<GopEncodeInfo> GetGopEncodeInfo(
      const GopStruct &gop_struct, const TplGopStats &tpl_gop_stats,
      const RefFrameTable &ref_frame_table_snapshot_init) = 0;
};
}  // namespace aom

#endif  // AOM_AV1_RATECTRL_QMODE_INTERFACE_H_
