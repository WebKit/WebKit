/*
 *  Copyright 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "api/peerconnection/RTCEncodedImage+Private.h"

#import <XCTest/XCTest.h>

@interface RTCEncodedImageTests : XCTestCase
@end

@implementation RTCEncodedImageTests

- (void)testInitializedWithNativeEncodedImage {
  const auto encoded_data = webrtc::EncodedImageBuffer::Create();
  webrtc::EncodedImage encoded_image;
  encoded_image.SetEncodedData(encoded_data);

  RTCEncodedImage *encodedImage =
      [[RTCEncodedImage alloc] initWithNativeEncodedImage:encoded_image];

  XCTAssertEqual([encodedImage nativeEncodedImage].GetEncodedData(), encoded_data);
}

- (void)testInitWithNSData {
  NSData *bufferData = [NSData data];
  RTCEncodedImage *encodedImage = [[RTCEncodedImage alloc] init];
  encodedImage.buffer = bufferData;

  webrtc::EncodedImage result_encoded_image = [encodedImage nativeEncodedImage];
  XCTAssertTrue(result_encoded_image.GetEncodedData() != nullptr);
  XCTAssertEqual(result_encoded_image.GetEncodedData()->data(), bufferData.bytes);
}

- (void)testRetainsNativeEncodedImage {
  RTCEncodedImage *encodedImage;
  {
    const auto encoded_data = webrtc::EncodedImageBuffer::Create();
    webrtc::EncodedImage encoded_image;
    encoded_image.SetEncodedData(encoded_data);
    encodedImage = [[RTCEncodedImage alloc] initWithNativeEncodedImage:encoded_image];
  }
  webrtc::EncodedImage result_encoded_image = [encodedImage nativeEncodedImage];
  XCTAssertTrue(result_encoded_image.GetEncodedData() != nullptr);
  XCTAssertTrue(result_encoded_image.GetEncodedData()->data() != nullptr);
}

@end
