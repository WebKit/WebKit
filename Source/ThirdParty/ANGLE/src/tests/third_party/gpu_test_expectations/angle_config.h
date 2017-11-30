//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// angle_config.h:
//   Helpers for importing the gpu test expectations package from Chrome.
//

#ifndef ANGLE_GPU_TEST_EXPECTATIONS_ANGLE_CONFIG_H_
#define ANGLE_GPU_TEST_EXPECTATIONS_ANGLE_CONFIG_H_

#include <stdint.h>

#include <iostream>

#include "common/debug.h"
#include "common/string_utils.h"

#define DCHECK_EQ(A,B) ASSERT((A) == (B))
#define DCHECK_NE(A,B) ASSERT((A) != (B))
#define DCHECK(X) ASSERT(X)
#define DLOG(X) std::cerr
#define LOG(X) std::cerr

#define GPU_EXPORT

// Shim Chromium's types by importing symbols in the correct namespaces
namespace base
{
    using angle::kWhitespaceASCII;
    using angle::TRIM_WHITESPACE;
    using angle::KEEP_WHITESPACE;
    using angle::SPLIT_WANT_ALL;
    using angle::SPLIT_WANT_NONEMPTY;
    using angle::SplitString;
    using angle::SplitStringAlongWhitespace;
    using angle::HexStringToUInt;
    using angle::ReadFileToString;

    using TimeDelta = int;
}  // namespace base

namespace gfx
{
    class Size
    {
      public:
        int width() const { return 0; }
        int height() const { return 0; }
    };
}  // namespace gfx

struct DxDiagNode
{
};

// TODO(jmadill): other platforms
// clang-format off
#if defined(_WIN32) || defined(_WIN64)
#    define OS_WIN
#elif defined(ANDROID)
#    define OS_ANDROID
#elif defined(__linux__)
#    define OS_LINUX
#elif defined(__APPLE__)
#    define OS_MACOSX
#else
#    error "Unsupported platform"
#endif
// clang-format on

#endif
