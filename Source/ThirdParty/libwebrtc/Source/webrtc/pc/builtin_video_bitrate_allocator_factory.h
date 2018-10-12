/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_BUILTIN_VIDEO_BITRATE_ALLOCATOR_FACTORY_H_
#define PC_BUILTIN_VIDEO_BITRATE_ALLOCATOR_FACTORY_H_

#include <memory>

#include "api/video/video_bitrate_allocator_factory.h"

namespace webrtc {

std::unique_ptr<VideoBitrateAllocatorFactory>
CreateBuiltinVideoBitrateAllocatorFactory();

}  // namespace webrtc

#endif  // PC_BUILTIN_VIDEO_BITRATE_ALLOCATOR_FACTORY_H_
