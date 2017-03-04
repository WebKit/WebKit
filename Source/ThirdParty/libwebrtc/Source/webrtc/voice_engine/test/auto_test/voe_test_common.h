/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_VOE_TEST_COMMON_H_
#define WEBRTC_VOICE_ENGINE_VOE_TEST_COMMON_H_

#ifdef WEBRTC_ANDROID
#include <android/log.h>
#define ANDROID_LOG_TAG "VoiceEngine Auto Test"
#define TEST_LOG(...) \
    __android_log_print(ANDROID_LOG_DEBUG, ANDROID_LOG_TAG, __VA_ARGS__)
#define TEST_LOG_ERROR(...) \
    __android_log_print(ANDROID_LOG_ERROR, ANDROID_LOG_TAG, __VA_ARGS__)
#define TEST_LOG_FLUSH
#else
#define TEST_LOG printf
#define TEST_LOG_ERROR printf
#define TEST_LOG_FLUSH fflush(NULL)
#endif

// Time in ms to test each packet size for each codec
#define CODEC_TEST_TIME 400

#endif  // WEBRTC_VOICE_ENGINE_VOE_TEST_COMMON_H_
