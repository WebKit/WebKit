/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>

#include <vector>

#include "rtc_base/gunit.h"

#import "api/peerconnection/RTCConfiguration+Private.h"
#import "api/peerconnection/RTCConfiguration.h"
#import "api/peerconnection/RTCIceServer.h"
#import "api/peerconnection/RTCMediaConstraints.h"
#import "api/peerconnection/RTCPeerConnection.h"
#import "api/peerconnection/RTCPeerConnectionFactory.h"
#import "helpers/NSString+StdString.h"

@interface RTCCertificateTest : NSObject
- (void)testCertificateIsUsedInConfig;
@end

@implementation RTCCertificateTest

- (void)testCertificateIsUsedInConfig {
  RTCConfiguration *originalConfig = [[RTCConfiguration alloc] init];

  NSArray *urlStrings = @[ @"stun:stun1.example.net" ];
  RTCIceServer *server = [[RTCIceServer alloc] initWithURLStrings:urlStrings];
  originalConfig.iceServers = @[ server ];

  // Generate a new certificate.
  RTCCertificate *originalCertificate = [RTCCertificate generateCertificateWithParams:@{
    @"expires" : @100000,
    @"name" : @"RSASSA-PKCS1-v1_5"
  }];

  // Store certificate in configuration.
  originalConfig.certificate = originalCertificate;

  RTCMediaConstraints *contraints =
      [[RTCMediaConstraints alloc] initWithMandatoryConstraints:@{} optionalConstraints:nil];
  RTCPeerConnectionFactory *factory = [[RTCPeerConnectionFactory alloc] init];

  // Create PeerConnection with this certificate.
  RTCPeerConnection *peerConnection =
      [factory peerConnectionWithConfiguration:originalConfig constraints:contraints delegate:nil];

  // Retrieve certificate from the configuration.
  RTCConfiguration *retrievedConfig = peerConnection.configuration;

  // Extract PEM strings from original certificate.
  std::string originalPrivateKeyField = [[originalCertificate private_key] UTF8String];
  std::string originalCertificateField = [[originalCertificate certificate] UTF8String];

  // Extract PEM strings from certificate retrieved from configuration.
  RTCCertificate *retrievedCertificate = retrievedConfig.certificate;
  std::string retrievedPrivateKeyField = [[retrievedCertificate private_key] UTF8String];
  std::string retrievedCertificateField = [[retrievedCertificate certificate] UTF8String];

  // Check that the original certificate and retrieved certificate match.
  EXPECT_EQ(originalPrivateKeyField, retrievedPrivateKeyField);
  EXPECT_EQ(retrievedCertificateField, retrievedCertificateField);
}

@end

TEST(CertificateTest, CertificateIsUsedInConfig) {
  RTCCertificateTest *test = [[RTCCertificateTest alloc] init];
  [test testCertificateIsUsedInConfig];
}
