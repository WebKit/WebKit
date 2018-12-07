/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCCryptoOptions.h"

@implementation RTCCryptoOptions

@synthesize srtpEnableGcmCryptoSuites = _srtpEnableGcmCryptoSuites;
@synthesize srtpEnableAes128Sha1_32CryptoCipher = _srtpEnableAes128Sha1_32CryptoCipher;
@synthesize srtpEnableEncryptedRtpHeaderExtensions = _srtpEnableEncryptedRtpHeaderExtensions;
@synthesize sframeRequireFrameEncryption = _sframeRequireFrameEncryption;

- (instancetype)initWithSrtpEnableGcmCryptoSuites:(BOOL)srtpEnableGcmCryptoSuites
              srtpEnableAes128Sha1_32CryptoCipher:(BOOL)srtpEnableAes128Sha1_32CryptoCipher
           srtpEnableEncryptedRtpHeaderExtensions:(BOOL)srtpEnableEncryptedRtpHeaderExtensions
                     sframeRequireFrameEncryption:(BOOL)sframeRequireFrameEncryption {
  if (self = [super init]) {
    _srtpEnableGcmCryptoSuites = srtpEnableGcmCryptoSuites;
    _srtpEnableAes128Sha1_32CryptoCipher = srtpEnableAes128Sha1_32CryptoCipher;
    _srtpEnableEncryptedRtpHeaderExtensions = srtpEnableEncryptedRtpHeaderExtensions;
    _sframeRequireFrameEncryption = sframeRequireFrameEncryption;
  }
  return self;
}

@end
