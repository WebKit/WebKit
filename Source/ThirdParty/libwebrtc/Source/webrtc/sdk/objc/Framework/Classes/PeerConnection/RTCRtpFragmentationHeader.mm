/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "WebRTC/RTCVideoCodec.h"

#include "modules/include/module_common_types.h"

@implementation RTCRtpFragmentationHeader

@synthesize fragmentationOffset = _fragmentationOffset;
@synthesize fragmentationLength = _fragmentationLength;
@synthesize fragmentationTimeDiff = _fragmentationTimeDiff;
@synthesize fragmentationPlType = _fragmentationPlType;

- (instancetype)initWithNativeFragmentationHeader:
        (const webrtc::RTPFragmentationHeader *)fragmentationHeader {
  if (self = [super init]) {
    if (fragmentationHeader) {
      int count = fragmentationHeader->fragmentationVectorSize;
      NSMutableArray *offsets = [NSMutableArray array];
      NSMutableArray *lengths = [NSMutableArray array];
      NSMutableArray *timeDiffs = [NSMutableArray array];
      NSMutableArray *plTypes = [NSMutableArray array];
      for (int i = 0; i < count; ++i) {
        [offsets addObject:@(fragmentationHeader->fragmentationOffset[i])];
        [lengths addObject:@(fragmentationHeader->fragmentationLength[i])];
        [timeDiffs addObject:@(fragmentationHeader->fragmentationTimeDiff[i])];
        [plTypes addObject:@(fragmentationHeader->fragmentationPlType[i])];
      }
      _fragmentationOffset = [offsets copy];
      _fragmentationLength = [lengths copy];
      _fragmentationTimeDiff = [timeDiffs copy];
      _fragmentationPlType = [plTypes copy];
    }
  }

  return self;
}

- (std::unique_ptr<webrtc::RTPFragmentationHeader>)createNativeFragmentationHeader {
  auto fragmentationHeader =
      std::unique_ptr<webrtc::RTPFragmentationHeader>(new webrtc::RTPFragmentationHeader);
  fragmentationHeader->VerifyAndAllocateFragmentationHeader(_fragmentationOffset.count);
  for (NSUInteger i = 0; i < _fragmentationOffset.count; ++i) {
    fragmentationHeader->fragmentationOffset[i] = (size_t)_fragmentationOffset[i].unsignedIntValue;
    fragmentationHeader->fragmentationLength[i] = (size_t)_fragmentationLength[i].unsignedIntValue;
    fragmentationHeader->fragmentationTimeDiff[i] =
        (uint16_t)_fragmentationOffset[i].unsignedIntValue;
    fragmentationHeader->fragmentationPlType[i] = (uint8_t)_fragmentationOffset[i].unsignedIntValue;
  }

  return fragmentationHeader;
}

@end
