/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface RTCIntervalRange : NSObject

@property(nonatomic, readonly) NSInteger min;
@property(nonatomic, readonly) NSInteger max;

- (instancetype)init;
- (instancetype)initWithMin:(NSInteger)min
                        max:(NSInteger)max
    NS_DESIGNATED_INITIALIZER;

@end

NS_ASSUME_NONNULL_END

