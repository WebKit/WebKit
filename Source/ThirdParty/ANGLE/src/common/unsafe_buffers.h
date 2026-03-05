//
// Copyright 2025 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// unsafe_buffers.h: exempts code from unsafe buffers warnings. Based upon
// https://source.chromium.org/chromium/chromium/src/+/main:base/compiler_specific.h

#ifndef COMMON_UNSAFE_BUFFERS_H_
#define COMMON_UNSAFE_BUFFERS_H_

#if defined(__clang__) && defined(__has_attribute)
#    if __has_attribute(unsafe_buffer_usage)
#        define ANGLE_UNSAFE_BUFFER_USAGE [[clang::unsafe_buffer_usage]]
#    endif
#endif
#if !defined(ANGLE_UNSAFE_BUFFER_USAGE)
#    define ANGLE_UNSAFE_BUFFER_USAGE
#endif

// Formatting is off so that we can put each _Pragma on its own line.
// clang-format off
#if defined(UNSAFE_BUFFERS_BUILD)
#    define ANGLE_UNSAFE_BUFFERS(...)                  \
         _Pragma("clang unsafe_buffer_usage begin")    \
         __VA_ARGS__                                   \
         _Pragma("clang unsafe_buffer_usage end")
#else
#    define ANGLE_UNSAFE_BUFFERS(...) __VA_ARGS__
#endif
// clang-format on

// Like ANGLE_UNSAFE_BUFFERS(), but indicates there is a TODO() task.
#define ANGLE_UNSAFE_TODO(...) ANGLE_UNSAFE_BUFFERS(__VA_ARGS__)

#endif  // COMMON_UNSAFE_BUFFERS_H_
