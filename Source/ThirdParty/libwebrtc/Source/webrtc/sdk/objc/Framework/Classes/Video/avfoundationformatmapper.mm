/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "avfoundationformatmapper.h"

#import "WebRTC/RTCLogging.h"

// TODO(denicija): add support for higher frame rates.
// See http://crbug/webrtc/6355 for more info.
static const int kFramesPerSecond = 30;

static inline BOOL IsMediaSubTypeSupported(FourCharCode mediaSubType) {
  return (mediaSubType == kCVPixelFormatType_420YpCbCr8PlanarFullRange ||
          mediaSubType == kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange);
}

static inline BOOL IsFrameRateWithinRange(int fps, AVFrameRateRange* range) {
  return range.minFrameRate <= fps && range.maxFrameRate >= fps;
}

// Returns filtered array of device formats based on predefined constraints our
// stack imposes.
static NSArray<AVCaptureDeviceFormat*>* GetEligibleDeviceFormats(
    const AVCaptureDevice* device,
    int supportedFps) {
  NSMutableArray<AVCaptureDeviceFormat*>* eligibleDeviceFormats =
      [NSMutableArray array];

  for (AVCaptureDeviceFormat* format in device.formats) {
    // Filter out subTypes that we currently don't support in the stack
    FourCharCode mediaSubType =
        CMFormatDescriptionGetMediaSubType(format.formatDescription);
    if (!IsMediaSubTypeSupported(mediaSubType)) {
      continue;
    }

    // Filter out frame rate ranges that we currently don't support in the stack
    for (AVFrameRateRange* frameRateRange in format.videoSupportedFrameRateRanges) {
      if (IsFrameRateWithinRange(supportedFps, frameRateRange)) {
        [eligibleDeviceFormats addObject:format];
        break;
      }
    }
  }

  return [eligibleDeviceFormats copy];
}

// Mapping from cricket::VideoFormat to AVCaptureDeviceFormat.
static AVCaptureDeviceFormat* GetDeviceFormatForVideoFormat(
    const AVCaptureDevice* device,
    const cricket::VideoFormat& videoFormat) {
  AVCaptureDeviceFormat* desiredDeviceFormat = nil;
  NSArray<AVCaptureDeviceFormat*>* eligibleFormats =
      GetEligibleDeviceFormats(device, videoFormat.framerate());

  for (AVCaptureDeviceFormat* deviceFormat in eligibleFormats) {
    CMVideoDimensions dimension =
        CMVideoFormatDescriptionGetDimensions(deviceFormat.formatDescription);
    FourCharCode mediaSubType =
        CMFormatDescriptionGetMediaSubType(deviceFormat.formatDescription);

    if (videoFormat.width == dimension.width &&
        videoFormat.height == dimension.height) {
      if (mediaSubType == kCVPixelFormatType_420YpCbCr8BiPlanarFullRange) {
        // This is the preferred format so no need to wait for better option.
        return deviceFormat;
      } else {
        // This is good candidate, but let's wait for something better.
        desiredDeviceFormat = deviceFormat;
      }
    }
  }

  return desiredDeviceFormat;
}
namespace webrtc {
std::set<cricket::VideoFormat> GetSupportedVideoFormatsForDevice(
    AVCaptureDevice* device) {
  std::set<cricket::VideoFormat> supportedFormats;

  NSArray<AVCaptureDeviceFormat*>* eligibleFormats =
      GetEligibleDeviceFormats(device, kFramesPerSecond);

  for (AVCaptureDeviceFormat* deviceFormat in eligibleFormats) {
    CMVideoDimensions dimension =
        CMVideoFormatDescriptionGetDimensions(deviceFormat.formatDescription);
    cricket::VideoFormat format = cricket::VideoFormat(
        dimension.width, dimension.height,
        cricket::VideoFormat::FpsToInterval(kFramesPerSecond),
        cricket::FOURCC_NV12);
    supportedFormats.insert(format);
  }

  return supportedFormats;
}

bool SetFormatForCaptureDevice(AVCaptureDevice* device,
                               AVCaptureSession* session,
                               const cricket::VideoFormat& format) {
  AVCaptureDeviceFormat* deviceFormat =
      GetDeviceFormatForVideoFormat(device, format);
  const int fps = cricket::VideoFormat::IntervalToFps(format.interval);

  NSError* error = nil;
  bool success = true;
  [session beginConfiguration];
  if ([device lockForConfiguration:&error]) {
    @try {
      device.activeFormat = deviceFormat;
      device.activeVideoMinFrameDuration = CMTimeMake(1, fps);
    } @catch (NSException* exception) {
      RTCLogError(@"Failed to set active format!\n User info:%@",
                  exception.userInfo);
      success = false;
    }

    [device unlockForConfiguration];
  } else {
    RTCLogError(@"Failed to lock device %@. Error: %@", device, error.userInfo);
    success = false;
  }
  [session commitConfiguration];

  return success;
}

}  // namespace webrtc
