/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/include/module_common_types.h"

#include <string.h>

#include <cstdint>
#include <utility>

#include "rtc_base/numerics/safe_conversions.h"

namespace webrtc {

RTPFragmentationHeader::RTPFragmentationHeader()
    : fragmentationVectorSize(0),
      fragmentationOffset(nullptr),
      fragmentationLength(nullptr) {}

RTPFragmentationHeader::RTPFragmentationHeader(RTPFragmentationHeader&& other)
    : RTPFragmentationHeader() {
  swap(*this, other);
}

RTPFragmentationHeader& RTPFragmentationHeader::operator=(
    RTPFragmentationHeader&& other) {
  swap(*this, other);
  return *this;
}

RTPFragmentationHeader::~RTPFragmentationHeader() {
  delete[] fragmentationOffset;
  delete[] fragmentationLength;
}

void swap(RTPFragmentationHeader& a, RTPFragmentationHeader& b) {
  using std::swap;
  swap(a.fragmentationVectorSize, b.fragmentationVectorSize);
  swap(a.fragmentationOffset, b.fragmentationOffset);
  swap(a.fragmentationLength, b.fragmentationLength);
}

void RTPFragmentationHeader::CopyFrom(const RTPFragmentationHeader& src) {
  if (this == &src) {
    return;
  }

  if (src.fragmentationVectorSize != fragmentationVectorSize) {
    // new size of vectors

    // delete old
    delete[] fragmentationOffset;
    fragmentationOffset = nullptr;
    delete[] fragmentationLength;
    fragmentationLength = nullptr;

    if (src.fragmentationVectorSize > 0) {
      // allocate new
      if (src.fragmentationOffset) {
        fragmentationOffset = new size_t[src.fragmentationVectorSize];
      }
      if (src.fragmentationLength) {
        fragmentationLength = new size_t[src.fragmentationVectorSize];
      }
    }
    // set new size
    fragmentationVectorSize = src.fragmentationVectorSize;
  }

  if (src.fragmentationVectorSize > 0) {
    // copy values
    if (src.fragmentationOffset) {
      memcpy(fragmentationOffset, src.fragmentationOffset,
             src.fragmentationVectorSize * sizeof(size_t));
    }
    if (src.fragmentationLength) {
      memcpy(fragmentationLength, src.fragmentationLength,
             src.fragmentationVectorSize * sizeof(size_t));
    }
  }
}

void RTPFragmentationHeader::Resize(size_t size) {
  const uint16_t size16 = rtc::dchecked_cast<uint16_t>(size);
  if (fragmentationVectorSize < size16) {
    uint16_t oldVectorSize = fragmentationVectorSize;
    {
      // offset
      size_t* oldOffsets = fragmentationOffset;
      fragmentationOffset = new size_t[size16];
      memset(fragmentationOffset + oldVectorSize, 0,
             sizeof(size_t) * (size16 - oldVectorSize));
      // copy old values
      memcpy(fragmentationOffset, oldOffsets, sizeof(size_t) * oldVectorSize);
      delete[] oldOffsets;
    }
    // length
    {
      size_t* oldLengths = fragmentationLength;
      fragmentationLength = new size_t[size16];
      memset(fragmentationLength + oldVectorSize, 0,
             sizeof(size_t) * (size16 - oldVectorSize));
      memcpy(fragmentationLength, oldLengths, sizeof(size_t) * oldVectorSize);
      delete[] oldLengths;
    }
    fragmentationVectorSize = size16;
  }
}

}  // namespace webrtc
