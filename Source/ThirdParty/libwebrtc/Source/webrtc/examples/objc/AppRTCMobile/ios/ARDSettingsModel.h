/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN
/**
 * Model class for user defined settings.
 *
 * Currently used for streaming media constraints and bitrate only.
 * In future audio media constraints support can be added as well.
 * Offers list of avaliable video resolutions that can construct streaming media constraint.
 * Exposes methods for reading and storing media constraints from persistent store.
 * Also translates current user defined media constraint into RTCMediaConstraints
 * dictionary.
 */
@interface ARDSettingsModel : NSObject

/**
 * Returns array of available capture resoultions.
 *
 * The capture resolutions are represented as strings in the following format
 * [width]x[height]
 */
- (NSArray<NSString *> *)availableVideoResoultionsMediaConstraints;

/**
 * Returns current video resolution media constraint string.
 * If no constraint is in store, default value of 640x480 is returned.
 * When defaulting to value, the default is saved in store for consistency reasons.
 */
- (NSString *)currentVideoResoultionConstraintFromStore;

/**
 * Stores the provided video resolution media constraint string into the store.
 *
 * If the provided constraint is no part of the available video resolutions
 * the store operation will not be executed and NO will be returned.
 * @param constraint the string to be stored.
 * @return YES/NO depending on success.
 */
- (BOOL)storeVideoResoultionConstraint:(NSString *)constraint;

/**
 * Converts the current media constraints from store into dictionary with RTCMediaConstraints
 * values.
 *
 * @return NSDictionary with RTC width and height parameters
 */
- (nullable NSDictionary *)currentMediaConstraintFromStoreAsRTCDictionary;

/**
 * Returns current max bitrate setting from store if present.
 */
- (nullable NSNumber *)currentMaxBitrateSettingFromStore;

/**
 * Stores the provided bitrate value into the store.
 *
 * @param bitrate NSNumber representation of the max bitrate value.
 */
- (void)storeMaxBitrateSetting:(nullable NSNumber *)bitrate;

@end
NS_ASSUME_NONNULL_END
