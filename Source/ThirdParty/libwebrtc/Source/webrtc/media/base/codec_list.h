/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MEDIA_BASE_CODEC_LIST_H_
#define MEDIA_BASE_CODEC_LIST_H_

#include <cstddef>
#include <span>
#include <vector>

#include "api/rtc_error.h"
#include "media/base/codec.h"

namespace webrtc {

class CodecList {
 public:
  using iterator = std::vector<Codec>::iterator;
  using const_iterator = std::vector<Codec>::const_iterator;
  using value_type = Codec;

  CodecList() = default;
  // Copy and assign are available.
  CodecList(const CodecList&) = default;
  CodecList& operator=(const CodecList&) = default;
  CodecList(CodecList&&) = default;
  CodecList& operator=(CodecList&&) = default;
  bool operator==(const CodecList& o) const { return codecs_ == o.codecs_; }

  // Creates a codec list on untrusted data. If successful, the
  // resulting CodecList satisfies all the CodecList invariants.
  static RTCErrorOr<CodecList> Create(std::span<const Codec> codecs);
  // Creates a codec list on trusted data. Only for use when
  // the codec list is generated from internal code.
  static CodecList CreateFromTrustedData(std::span<const Codec> codecs) {
    return CodecList(codecs);
  }
  // The outcome of PushIfNotPresent().
  enum class PushResult {
    // The codec was added to the list.
    kInserted,
    // The exact same codec was already in the list; the list is unchanged.
    kDuplicate,
    // A different codec with the same payload type was already in the list.
    // The list is unchanged and an error is logged. This means that the
    // caller combined codecs that come from different payload type mappings,
    // which is a programming error.
    kConflict,
  };

  // Inserts a codec into the list if no codec with the same payload type is
  // present. The payload types in the list are kept unique; the codec that is
  // already in the list is never replaced.
  [[nodiscard]] PushResult PushIfNotPresent(const Codec& codec);

  // Vector-compatible API to access the codecs.
  iterator begin() { return codecs_.begin(); }
  iterator end() { return codecs_.end(); }
  const_iterator begin() const { return codecs_.begin(); }
  const_iterator end() const { return codecs_.end(); }
  const Codec& operator[](size_t i) const { return codecs_[i]; }
  Codec& operator[](size_t i) { return codecs_[i]; }
  // Appends a codec to the list. Payload types must remain unique. Note that
  // an RTX codec may be added before the codec that it refers to, so the check
  // that referenced codecs exist is left to CheckConsistency().
  void push_back(const Codec& codec);
  bool empty() const { return codecs_.empty(); }
  void clear() { codecs_.clear(); }
  size_t size() const { return codecs_.size(); }
  // Access to the whole codec list
  const std::vector<Codec>& codecs() const { return codecs_; }
  std::vector<Codec>& writable_codecs() { return codecs_; }
  // Verify consistency of a complete codec list.
  // Examples: checking that all RTX codecs have APT pointing
  // to a codec in the list.
  // The function will CHECK or DCHECK on inconsistencies. It must only be
  // called once the list is fully assembled.
  void CheckConsistency();

  // Marks the list as complete and moves the codecs out of it.
  //
  // This verifies the invariants that only hold for a complete list, such as
  // every RTX codec referring to a codec that is present, and returns an error
  // if they are violated. Use this rather than codecs() when handing the
  // codecs to a caller, so that the verification cannot be forgotten. Unlike
  // CheckConsistency(), an inconsistent list is reported as an error instead
  // of being fatal, since a list can become inconsistent as the result of the
  // codecs an application supplied.
  //
  // The CodecList is left empty, whether or not the check succeeded.
  [[nodiscard]] RTCErrorOr<std::vector<Codec>> Finalize() &&;

  template <typename Sink>
  friend void AbslStringify(Sink& sink, const CodecList& list) {
    absl::Format(&sink, "\n--- Codec list of size %d\n", list.size());
    for (const Codec& codec : list) {
      absl::Format(&sink, "%v\n", codec);
    }
    sink.Append("--- End\n");
  }

 private:
  // Creates a codec list on trusted data.
  explicit CodecList(std::span<const Codec> codecs)
      : codecs_(codecs.begin(), codecs.end()) {
    CheckConsistency();
  }

  std::vector<Codec> codecs_;
};

}  //  namespace webrtc


#endif  // MEDIA_BASE_CODEC_LIST_H_
