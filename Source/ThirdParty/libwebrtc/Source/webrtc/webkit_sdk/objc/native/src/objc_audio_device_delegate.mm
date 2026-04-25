/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <AudioUnit/AudioUnit.h>
#import <Foundation/Foundation.h>

#import "objc_audio_device.h"
#import "objc_audio_device_delegate.h"

#include "api/make_ref_counted.h"
#include "api/ref_counted_base.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/thread.h"

namespace {

constexpr double kPreferredInputSampleRate = 48000.0;
constexpr double kPreferredOutputSampleRate = 48000.0;

// WebRTC processes audio in chunks of 10ms. Preferring 20ms audio chunks
// is a compromize between performance and power consumption.
constexpr NSTimeInterval kPeferredInputIOBufferDuration = 0.02;
constexpr NSTimeInterval kPeferredOutputIOBufferDuration = 0.02;

class AudioDeviceDelegateImpl final : public rtc::RefCountedNonVirtual<AudioDeviceDelegateImpl> {
 public:
  AudioDeviceDelegateImpl(
      rtc::scoped_refptr<webrtc::objc_adm::ObjCAudioDeviceModule> audio_device_module,
      rtc::Thread* thread)
      : audio_device_module_(audio_device_module), thread_(thread) {
    RTC_DCHECK(audio_device_module_);
    RTC_DCHECK(thread_);
  }

  webrtc::objc_adm::ObjCAudioDeviceModule* audio_device_module() const {
    return audio_device_module_.get();
  }

  rtc::Thread* thread() const { return thread_; }

  void reset_audio_device_module() { audio_device_module_ = nullptr; }

 private:
  rtc::scoped_refptr<webrtc::objc_adm::ObjCAudioDeviceModule> audio_device_module_;
  rtc::Thread* thread_;
};

}  // namespace

@implementation ObjCAudioDeviceDelegate {
  rtc::scoped_refptr<AudioDeviceDelegateImpl> impl_;
}

@synthesize getPlayoutData = getPlayoutData_;

@synthesize deliverRecordedData = deliverRecordedData_;

@synthesize preferredInputSampleRate = preferredInputSampleRate_;

@synthesize preferredInputIOBufferDuration = preferredInputIOBufferDuration_;

@synthesize preferredOutputSampleRate = preferredOutputSampleRate_;

@synthesize preferredOutputIOBufferDuration = preferredOutputIOBufferDuration_;

- (instancetype)initWithAudioDeviceModule:
                    (rtc::scoped_refptr<webrtc::objc_adm::ObjCAudioDeviceModule>)audioDeviceModule
                        audioDeviceThread:(rtc::Thread*)thread {
  RTC_DCHECK_RUN_ON(thread);
  if (self = [super init]) {
    impl_ = rtc::make_ref_counted<AudioDeviceDelegateImpl>(audioDeviceModule, thread);
    preferredInputSampleRate_ = kPreferredInputSampleRate;
    preferredInputIOBufferDuration_ = kPeferredInputIOBufferDuration;
    preferredOutputSampleRate_ = kPreferredOutputSampleRate;
    preferredOutputIOBufferDuration_ = kPeferredOutputIOBufferDuration;

    rtc::scoped_refptr<AudioDeviceDelegateImpl> playout_delegate = impl_;
    getPlayoutData_ = ^OSStatus(AudioUnitRenderActionFlags* _Nonnull actionFlags,
                                const AudioTimeStamp* _Nonnull timestamp,
                                NSInteger inputBusNumber,
                                UInt32 frameCount,
                                AudioBufferList* _Nonnull outputData) {
      webrtc::objc_adm::ObjCAudioDeviceModule* audio_device =
          playout_delegate->audio_device_module();
      if (audio_device) {
        return audio_device->OnGetPlayoutData(
            actionFlags, timestamp, inputBusNumber, frameCount, outputData);
      } else {
        *actionFlags |= kAudioUnitRenderAction_OutputIsSilence;
        RTC_LOG(LS_VERBOSE) << "No alive audio device";
        return noErr;
      }
    };

    rtc::scoped_refptr<AudioDeviceDelegateImpl> record_delegate = impl_;
    deliverRecordedData_ =
        ^OSStatus(AudioUnitRenderActionFlags* _Nonnull actionFlags,
                  const AudioTimeStamp* _Nonnull timestamp,
                  NSInteger inputBusNumber,
                  UInt32 frameCount,
                  const AudioBufferList* _Nullable inputData,
                  void* renderContext,
                  RTC_OBJC_TYPE(RTCAudioDeviceRenderRecordedDataBlock) _Nullable renderBlock) {
          webrtc::objc_adm::ObjCAudioDeviceModule* audio_device =
              record_delegate->audio_device_module();
          if (audio_device) {
            return audio_device->OnDeliverRecordedData(actionFlags,
                                                       timestamp,
                                                       inputBusNumber,
                                                       frameCount,
                                                       inputData,
                                                       renderContext,
                                                       renderBlock);
          } else {
            RTC_LOG(LS_VERBOSE) << "No alive audio device";
            return noErr;
          }
        };
  }
  return self;
}

- (void)notifyAudioInputParametersChange {
  RTC_DCHECK_RUN_ON(impl_->thread());
  webrtc::objc_adm::ObjCAudioDeviceModule* audio_device_module = impl_->audio_device_module();
  if (audio_device_module) {
    audio_device_module->HandleAudioInputParametersChange();
  }
}

- (void)notifyAudioOutputParametersChange {
  RTC_DCHECK_RUN_ON(impl_->thread());
  webrtc::objc_adm::ObjCAudioDeviceModule* audio_device_module = impl_->audio_device_module();
  if (audio_device_module) {
    audio_device_module->HandleAudioOutputParametersChange();
  }
}

- (void)notifyAudioInputInterrupted {
  RTC_DCHECK_RUN_ON(impl_->thread());
  webrtc::objc_adm::ObjCAudioDeviceModule* audio_device_module = impl_->audio_device_module();
  if (audio_device_module) {
    audio_device_module->HandleAudioInputInterrupted();
  }
}

- (void)notifyAudioOutputInterrupted {
  RTC_DCHECK_RUN_ON(impl_->thread());
  webrtc::objc_adm::ObjCAudioDeviceModule* audio_device_module = impl_->audio_device_module();
  if (audio_device_module) {
    audio_device_module->HandleAudioOutputInterrupted();
  }
}

- (void)dispatchAsync:(dispatch_block_t)block {
  rtc::Thread* thread = impl_->thread();
  RTC_DCHECK(thread);
  thread->PostTask([block] {
    @autoreleasepool {
      block();
    }
  });
}

- (void)dispatchSync:(dispatch_block_t)block {
  rtc::Thread* thread = impl_->thread();
  RTC_DCHECK(thread);
  if (thread->IsCurrent()) {
    @autoreleasepool {
      block();
    }
  } else {
    thread->BlockingCall([block] {
      @autoreleasepool {
        block();
      }
    });
  }
}

- (void)resetAudioDeviceModule {
  RTC_DCHECK_RUN_ON(impl_->thread());
  impl_->reset_audio_device_module();
}

@end
