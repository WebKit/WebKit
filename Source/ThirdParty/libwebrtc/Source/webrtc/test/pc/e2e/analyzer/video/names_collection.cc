/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/analyzer/video/names_collection.h"

#include <optional>
#include <set>

#include "absl/strings/string_view.h"

namespace webrtc {

NamesCollection::NamesCollection(rtc::ArrayView<const std::string> names) {
  names_ = std::vector<std::string>(names.begin(), names.end());
  for (size_t i = 0; i < names_.size(); ++i) {
    index_.emplace(names_[i], i);
    removed_.emplace_back(false);
  }
  size_ = names_.size();
}

bool NamesCollection::HasName(absl::string_view name) const {
  auto it = index_.find(name);
  if (it == index_.end()) {
    return false;
  }
  return !removed_[it->second];
}

size_t NamesCollection::AddIfAbsent(absl::string_view name) {
  auto it = index_.find(name);
  if (it != index_.end()) {
    // Name was registered in the collection before: we need to restore it.
    size_t index = it->second;
    if (removed_[index]) {
      removed_[index] = false;
      size_++;
    }
    return index;
  }
  size_t out = names_.size();
  size_t old_capacity = names_.capacity();
  names_.emplace_back(name);
  removed_.emplace_back(false);
  size_++;
  size_t new_capacity = names_.capacity();

  if (old_capacity == new_capacity) {
    index_.emplace(names_[out], out);
  } else {
    // Reallocation happened in the vector, so we need to rebuild `index_` to
    // fix absl::string_view internal references.
    index_.clear();
    for (size_t i = 0; i < names_.size(); ++i) {
      index_.emplace(names_[i], i);
    }
  }
  return out;
}

std::optional<size_t> NamesCollection::RemoveIfPresent(absl::string_view name) {
  auto it = index_.find(name);
  if (it == index_.end()) {
    return std::nullopt;
  }
  size_t index = it->second;
  if (removed_[index]) {
    return std::nullopt;
  }
  removed_[index] = true;
  size_--;
  return index;
}

std::set<size_t> NamesCollection::GetPresentIndexes() const {
  std::set<size_t> out;
  for (size_t i = 0; i < removed_.size(); ++i) {
    if (!removed_[i]) {
      out.insert(i);
    }
  }
  return out;
}

std::set<size_t> NamesCollection::GetAllIndexes() const {
  std::set<size_t> out;
  for (size_t i = 0; i < names_.size(); ++i) {
    out.insert(i);
  }
  return out;
}

}  // namespace webrtc
