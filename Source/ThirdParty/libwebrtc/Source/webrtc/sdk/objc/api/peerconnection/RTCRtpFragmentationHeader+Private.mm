/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCRtpFragmentationHeader+Private.h"

#include "modules/include/module_common_types.h"

@implementation RTCRtpFragmentationHeader (Private)

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
      self.fragmentationOffset = [offsets copy];
      self.fragmentationLength = [lengths copy];
      self.fragmentationTimeDiff = [timeDiffs copy];
      self.fragmentationPlType = [plTypes copy];
    }
  }

  return self;
}

- (std::unique_ptr<webrtc::RTPFragmentationHeader>)createNativeFragmentationHeader {
  auto fragmentationHeader =
      std::unique_ptr<webrtc::RTPFragmentationHeader>(new webrtc::RTPFragmentationHeader);
  fragmentationHeader->VerifyAndAllocateFragmentationHeader(self.fragmentationOffset.count);
  for (NSUInteger i = 0; i < self.fragmentationOffset.count; ++i) {
    fragmentationHeader->fragmentationOffset[i] = (size_t)self.fragmentationOffset[i].unsignedIntValue;
    fragmentationHeader->fragmentationLength[i] = (size_t)self.fragmentationLength[i].unsignedIntValue;
    fragmentationHeader->fragmentationTimeDiff[i] =
        (uint16_t)self.fragmentationOffset[i].unsignedIntValue;
    fragmentationHeader->fragmentationPlType[i] = (uint8_t)self.fragmentationOffset[i].unsignedIntValue;
  }

  return fragmentationHeader;
}

@end
