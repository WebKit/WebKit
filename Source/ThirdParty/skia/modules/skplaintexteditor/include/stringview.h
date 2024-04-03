// Copyright 2019 Google LLC.
// Use of this source code is governed by a BSD-style license that can be found in the LICENSE file.
#ifndef stringview_DEFINED
#define stringview_DEFINED

#include <cstddef>

namespace SkPlainTextEditor {

template <typename T>
struct Span {
    T* data;
    std::size_t size;
};

using StringView = Span<const char>;

}  // namespace SkPlainTextEditor
#endif  // stringview_DEFINED
