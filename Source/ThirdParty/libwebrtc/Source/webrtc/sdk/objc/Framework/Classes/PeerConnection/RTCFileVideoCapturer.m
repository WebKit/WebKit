/**
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "WebRTC/RTCFileVideoCapturer.h"

#import "WebRTC/RTCLogging.h"
#import "WebRTC/RTCVideoFrameBuffer.h"

NSString *const kRTCFileVideoCapturerErrorDomain = @"org.webrtc.RTCFileVideoCapturer";

typedef NS_ENUM(NSInteger, RTCFileVideoCapturerErrorCode) {
  RTCFileVideoCapturerErrorCode_CapturerRunning = 2000,
  RTCFileVideoCapturerErrorCode_FileNotFound
};

typedef NS_ENUM(NSInteger, RTCFileVideoCapturerStatus) {
  RTCFileVideoCapturerStatusNotInitialized,
  RTCFileVideoCapturerStatusStarted,
  RTCFileVideoCapturerStatusStopped
};

@implementation RTCFileVideoCapturer {
  AVAssetReader *_reader;
  AVAssetReaderTrackOutput *_outTrack;
  RTCFileVideoCapturerStatus _status;
  CMTime _lastPresentationTime;
  dispatch_queue_t _frameQueue;
  NSURL *_fileURL;
}

- (void)startCapturingFromFileNamed:(NSString *)nameOfFile
                            onError:(RTCFileVideoCapturerErrorBlock)errorBlock {
  if (_status == RTCFileVideoCapturerStatusStarted) {
    NSError *error =
        [NSError errorWithDomain:kRTCFileVideoCapturerErrorDomain
                            code:RTCFileVideoCapturerErrorCode_CapturerRunning
                        userInfo:@{NSUnderlyingErrorKey : @"Capturer has been started."}];

    errorBlock(error);
    return;
  } else {
    _status = RTCFileVideoCapturerStatusStarted;
  }

  dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
    NSString *pathForFile = [self pathForFileName:nameOfFile];
    if (!pathForFile) {
      NSString *errorString =
          [NSString stringWithFormat:@"File %@ not found in bundle", nameOfFile];
      NSError *error = [NSError errorWithDomain:kRTCFileVideoCapturerErrorDomain
                                           code:RTCFileVideoCapturerErrorCode_FileNotFound
                                       userInfo:@{NSUnderlyingErrorKey : errorString}];
      errorBlock(error);
      return;
    }

    _lastPresentationTime = CMTimeMake(0, 0);

    _fileURL = [NSURL fileURLWithPath:pathForFile];
    [self setupReaderOnError:errorBlock];
  });
}

- (void)setupReaderOnError:(RTCFileVideoCapturerErrorBlock)errorBlock {
  AVURLAsset *asset = [AVURLAsset URLAssetWithURL:_fileURL options:nil];

  NSArray *allTracks = [asset tracksWithMediaType:AVMediaTypeVideo];
  NSError *error = nil;

  _reader = [[AVAssetReader alloc] initWithAsset:asset error:&error];
  if (error) {
    errorBlock(error);
    return;
  }

  NSDictionary *options = @{
    (NSString *)kCVPixelBufferPixelFormatTypeKey : @(kCVPixelFormatType_420YpCbCr8BiPlanarFullRange)
  };
  _outTrack =
      [[AVAssetReaderTrackOutput alloc] initWithTrack:allTracks.firstObject outputSettings:options];
  [_reader addOutput:_outTrack];

  [_reader startReading];
  RTCLog(@"File capturer started reading");
  [self readNextBuffer];
}
- (void)stopCapture {
  _status = RTCFileVideoCapturerStatusStopped;
  RTCLog(@"File capturer stopped.");
}

#pragma mark - Private

- (nullable NSString *)pathForFileName:(NSString *)fileName {
  NSArray *nameComponents = [fileName componentsSeparatedByString:@"."];
  if (nameComponents.count != 2) {
    return nil;
  }

  NSString *path =
      [[NSBundle mainBundle] pathForResource:nameComponents[0] ofType:nameComponents[1]];
  return path;
}

- (dispatch_queue_t)frameQueue {
  if (!_frameQueue) {
    _frameQueue = dispatch_queue_create("org.webrtc.filecapturer.video", DISPATCH_QUEUE_SERIAL);
    dispatch_set_target_queue(_frameQueue,
                              dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_BACKGROUND, 0));
  }
  return _frameQueue;
}

- (void)readNextBuffer {
  if (_status == RTCFileVideoCapturerStatusStopped) {
    [_reader cancelReading];
    _reader = nil;
    return;
  }

  if (_reader.status == AVAssetReaderStatusCompleted) {
    [_reader cancelReading];
    _reader = nil;
    [self setupReaderOnError:nil];
    return;
  }

  CMSampleBufferRef sampleBuffer = [_outTrack copyNextSampleBuffer];
  if (!sampleBuffer) {
    [self readNextBuffer];
    return;
  }
  if (CMSampleBufferGetNumSamples(sampleBuffer) != 1 || !CMSampleBufferIsValid(sampleBuffer) ||
      !CMSampleBufferDataIsReady(sampleBuffer)) {
    [self readNextBuffer];
    return;
  }

  [self publishSampleBuffer:sampleBuffer];
}

- (void)publishSampleBuffer:(CMSampleBufferRef)sampleBuffer {
  CMTime presentationTime = CMSampleBufferGetPresentationTimeStamp(sampleBuffer);
  Float64 presentationDifference =
      CMTimeGetSeconds(CMTimeSubtract(presentationTime, _lastPresentationTime));
  _lastPresentationTime = presentationTime;
  int64_t presentationDifferenceRound = lroundf(presentationDifference * NSEC_PER_SEC);

  __block dispatch_source_t timer = [self createStrictTimer];
  // Strict timer that will fire |presentationDifferenceRound| ns from now and never again.
  dispatch_source_set_timer(timer,
                            dispatch_time(DISPATCH_TIME_NOW, presentationDifferenceRound),
                            DISPATCH_TIME_FOREVER,
                            0);
  dispatch_source_set_event_handler(timer, ^{
    dispatch_source_cancel(timer);
    timer = nil;

    CVPixelBufferRef pixelBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);
    if (!pixelBuffer) {
      CFRelease(sampleBuffer);
      dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        [self readNextBuffer];
      });
      return;
    }

    RTCCVPixelBuffer *rtcPixelBuffer = [[RTCCVPixelBuffer alloc] initWithPixelBuffer:pixelBuffer];
    NSTimeInterval timeStampSeconds = CACurrentMediaTime();
    int64_t timeStampNs = lroundf(timeStampSeconds * NSEC_PER_SEC);
    RTCVideoFrame *videoFrame =
        [[RTCVideoFrame alloc] initWithBuffer:rtcPixelBuffer rotation:0 timeStampNs:timeStampNs];
    CFRelease(sampleBuffer);

    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
      [self readNextBuffer];
    });

    [self.delegate capturer:self didCaptureVideoFrame:videoFrame];
  });
  dispatch_activate(timer);
}

- (dispatch_source_t)createStrictTimer {
  dispatch_source_t timer = dispatch_source_create(
      DISPATCH_SOURCE_TYPE_TIMER, 0, DISPATCH_TIMER_STRICT, [self frameQueue]);
  return timer;
}

- (void)dealloc {
  [self stopCapture];
}

@end
