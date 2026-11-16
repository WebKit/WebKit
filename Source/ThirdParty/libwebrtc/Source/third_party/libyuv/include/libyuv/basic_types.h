/*
 *  Copyright 2011 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef INCLUDE_LIBYUV_BASIC_TYPES_H_
#define INCLUDE_LIBYUV_BASIC_TYPES_H_

#if !defined(LIBYUV_API)
#if defined(_WIN32) || defined(__CYGWIN__)
#if defined(LIBYUV_BUILDING_SHARED_LIBRARY)
#define LIBYUV_API __declspec(dllexport)
#elif defined(LIBYUV_USING_SHARED_LIBRARY)
#define LIBYUV_API __declspec(dllimport)
#else
#define LIBYUV_API
#endif  // LIBYUV_BUILDING_SHARED_LIBRARY
#elif defined(__GNUC__) && (__GNUC__ >= 4) && !defined(__APPLE__) && \
    (defined(LIBYUV_BUILDING_SHARED_LIBRARY) ||                      \
     defined(LIBYUV_USING_SHARED_LIBRARY))
#define LIBYUV_API __attribute__((visibility("default")))
#else
#define LIBYUV_API
#endif  // __GNUC__
#endif  // LIBYUV_API

#endif  // INCLUDE_LIBYUV_BASIC_TYPES_H_
