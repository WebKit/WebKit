// Copyright 2016 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Output buffer for WOFF2 decompression.

// Copyright 2016 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Output buffer for WOFF2 decompression.

#ifndef WOFF2_WOFF2_OUT_H_
#define WOFF2_WOFF2_OUT_H_

#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include "./port.h"

namespace woff2 {

// Suggested max size for output.
const size_t kDefaultMaxSize = 30 * 1024 * 1024;

using std::string;


/**
 * Output interface for the woff2 decoding.
 *
 * Writes to arbitrary offsets are supported to facilitate updating offset
 * table and checksums after tables are ready. Reading the current size is
 * supported so a 'loca' table can be built up while writing glyphs.
 *
 * By default limits size to kDefaultMaxSize.
 */
class WOFF2Out {
 public:
  virtual ~WOFF2Out(void) {}

  // Append n bytes of data from buf.
  // Return true if all written, false otherwise.
  virtual bool Write(const void *buf, size_t n) = 0;

  // Write n bytes of data from buf at offset.
  // Return true if all written, false otherwise.
  virtual bool Write(const void *buf, size_t offset, size_t n) = 0;

  virtual size_t Size() = 0;
};

/**
 * Expanding memory block for woff2 out. By default limited to kDefaultMaxSize.
 */
class WOFF2StringOut : public WOFF2Out {
 public:
  // Create a writer that writes its data to buf.
  // buf->size() will grow to at most max_size
  // buf may be sized (e.g. using EstimateWOFF2FinalSize) or empty.
  explicit WOFF2StringOut(string* buf);

  bool Write(const void *buf, size_t n) override;
  bool Write(const void *buf, size_t offset, size_t n) override;
  size_t Size() override { return offset_; }
  size_t MaxSize() { return max_size_; }
  void SetMaxSize(size_t max_size);
 private:
  string* buf_;
  size_t max_size_;
  size_t offset_;
};

/**
 * Fixed memory block for woff2 out.
 */
class WOFF2MemoryOut : public WOFF2Out {
 public:
  // Create a writer that writes its data to buf.
  WOFF2MemoryOut(uint8_t* buf, size_t buf_size);

  bool Write(const void *buf, size_t n) override;
  bool Write(const void *buf, size_t offset, size_t n) override;
  size_t Size() override { return offset_; }
 private:
  uint8_t* buf_;
  size_t buf_size_;
  size_t offset_;
};

} // namespace woff2

#endif  // WOFF2_WOFF2_OUT_H_
