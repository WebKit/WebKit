/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <set>

#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>

#include "media/base/videocapturer.h"

namespace webrtc {
// Mapping from AVCaptureDeviceFormat to cricket::VideoFormat for given input
// device.
std::set<cricket::VideoFormat> GetSupportedVideoFormatsForDevice(
    AVCaptureDevice* device);

// Sets device format for the provided capture device. Returns YES/NO depending
// on success.
bool SetFormatForCaptureDevice(AVCaptureDevice* device,
                               AVCaptureSession* session,
                               const cricket::VideoFormat& format);
}
