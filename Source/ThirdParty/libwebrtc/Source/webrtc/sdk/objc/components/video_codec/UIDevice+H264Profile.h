/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <UIKit/UIKit.h>

#include "media/base/h264_profile_level_id.h"

@interface UIDevice (H264Profile)

+ (absl::optional<webrtc::H264::ProfileLevelId>)maxSupportedH264Profile;

@end
