/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/libjingle/xmllite/qname.h"

namespace buzz {

QName::QName() {
}

QName::QName(const QName& qname)
    : namespace_(qname.namespace_),
      local_part_(qname.local_part_) {
}

QName::QName(const StaticQName& const_value)
    : namespace_(const_value.ns),
      local_part_(const_value.local) {
}

QName::QName(const std::string& ns, const std::string& local)
    : namespace_(ns),
      local_part_(local) {
}

QName::QName(const std::string& merged_or_local) {
  size_t i = merged_or_local.rfind(':');
  if (i == std::string::npos) {
    local_part_ = merged_or_local;
  } else {
    namespace_ = merged_or_local.substr(0, i);
    local_part_ = merged_or_local.substr(i + 1);
  }
}

QName::~QName() {
}

std::string QName::Merged() const {
  if (namespace_[0] == '\0')
    return local_part_;

  std::string result;
  result.reserve(namespace_.length() + 1 + local_part_.length());
  result += namespace_;
  result += ':';
  result += local_part_;
  return result;
}

bool QName::IsEmpty() const {
  return namespace_.empty() && local_part_.empty();
}

int QName::Compare(const StaticQName& other) const {
  int result = local_part_.compare(other.local);
  if (result != 0)
    return result;

  return namespace_.compare(other.ns);
}

int QName::Compare(const QName& other) const {
  int result = local_part_.compare(other.local_part_);
  if (result != 0)
    return result;

  return namespace_.compare(other.namespace_);
}

}  // namespace buzz
