/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "ARDSettingsStore.h"

static NSString *const kMediaConstraintsKey = @"rtc_video_resolution_media_constraints_key";
static NSString *const kBitrateKey = @"rtc_max_bitrate_key";

NS_ASSUME_NONNULL_BEGIN
@interface ARDSettingsStore () {
  NSUserDefaults *_storage;
}
@property(nonatomic, strong) NSUserDefaults *storage;
@end

@implementation ARDSettingsStore

- (NSUserDefaults *)storage {
  if (!_storage) {
    _storage = [NSUserDefaults standardUserDefaults];
  }
  return _storage;
}

- (nullable NSString *)videoResolutionConstraints {
  return [self.storage objectForKey:kMediaConstraintsKey];
}

- (void)setVideoResolutionConstraints:(NSString *)constraintsString {
  [self.storage setObject:constraintsString forKey:kMediaConstraintsKey];
  [self.storage synchronize];
}

- (nullable NSNumber *)maxBitrate {
  return [self.storage objectForKey:kBitrateKey];
}

- (void)setMaxBitrate:(nullable NSNumber *)value {
  [self.storage setObject:value forKey:kBitrateKey];
  [self.storage synchronize];
}

@end
NS_ASSUME_NONNULL_END
