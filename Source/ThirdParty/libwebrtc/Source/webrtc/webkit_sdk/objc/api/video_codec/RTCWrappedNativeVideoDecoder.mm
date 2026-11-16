/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>

#import "RTCWrappedNativeVideoDecoder.h"
#import "helpers/NSString+StdString.h"

@implementation RTCWrappedNativeVideoDecoder {
  std::unique_ptr<webrtc::VideoDecoder> _wrappedDecoder;
}

- (instancetype)initWithNativeDecoder:(std::unique_ptr<webrtc::VideoDecoder>)decoder {
  if (self = [super init]) {
    _wrappedDecoder = std::move(decoder);
  }

  return self;
}

- (std::unique_ptr<webrtc::VideoDecoder>)releaseWrappedDecoder {
  return std::move(_wrappedDecoder);
}

#pragma mark - RTCVideoDecoder

- (void)setCallback:(RTCVideoDecoderCallback)callback {
  RTC_DCHECK_NOTREACHED();
}

- (NSInteger)startDecodeWithNumberOfCores:(int)numberOfCores {
  RTC_DCHECK_NOTREACHED();
  return 0;
}

- (NSInteger)releaseDecoder {
  RTC_DCHECK_NOTREACHED();
  return 0;
}

- (NSInteger)decode:(RTCEncodedImage *)encodedImage
        missingFrames:(BOOL)missingFrames
    codecSpecificInfo:(nullable id<RTCCodecSpecificInfo>)info
         renderTimeMs:(int64_t)renderTimeMs {
  RTC_DCHECK_NOTREACHED();
  return 0;
}

- (NSString *)implementationName {
#if !defined(WEBRTC_WEBKIT_BUILD)
  RTC_DCHECK_NOTREACHED();
#endif
  return nil;
}

@end
