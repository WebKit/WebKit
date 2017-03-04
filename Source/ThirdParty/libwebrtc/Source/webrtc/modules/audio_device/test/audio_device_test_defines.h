/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_AUDIO_DEVICE_AUDIO_DEVICE_TEST_DEFINES_H
#define WEBRTC_AUDIO_DEVICE_AUDIO_DEVICE_TEST_DEFINES_H

#include "webrtc/common_types.h"
#include "webrtc/modules/audio_device/include/audio_device.h"
#include "webrtc/system_wrappers/include/trace.h"

#ifdef _WIN32
#define MACRO_DEFAULT_DEVICE AudioDeviceModule::kDefaultDevice
#define MACRO_DEFAULT_COMMUNICATION_DEVICE AudioDeviceModule::kDefaultCommunicationDevice
#else
#define MACRO_DEFAULT_DEVICE 0
#define MACRO_DEFAULT_COMMUNICATION_DEVICE 0
#endif

#ifdef ANDROID
#include <android/log.h>
#define LOG_TAG "WebRtc ADM TEST"
#define TEST_LOG(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define TEST_LOG_ERROR(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#define TEST_LOG printf
#define TEST_LOG_ERROR(...) fprintf(stderr, __VA_ARGS__)
#endif

static int warningCount = 0;

#define RESET_TEST                                              \
    do {                                                        \
        warningCount = 0;                                       \
    } while(0)                                                  \

#define PRINT_ERR_MSG(msg)                                      \
    do {                                                        \
        TEST_LOG_ERROR("Error at line %i of %s\n%s",            \
            __LINE__, __FILE__, msg);                           \
    } while(0)

#define WARNING(expr)                                           \
    do {                                                        \
        if (!(expr)) {                                          \
            TEST_LOG_ERROR("WARNING #%d: at line %i\n\n",       \
                           warningCount+1, __LINE__);           \
            warningCount++;                                     \
        }                                                       \
    } while(0)

#define PRINT_TEST_RESULTS                                      \
    do {                                                        \
        if (warningCount > 0)                                   \
        {                                                       \
            TEST_LOG(">> %d warnings <<\n\n",                   \
                     warningCount);                             \
        }                                                       \
    } while(0)

// Helper functions
// For iPhone, they are defined in iPhone specific test code.
// For Android, they are defined in API test only (since both
//   API and Func tests are built into the same lib).
// For other, they are defined in both API test and Func test.
const char* GetFilename(const char* filename);
const char* GetResource(const char* resource);

#endif  // WEBRTC_AUDIO_DEVICE_AUDIO_DEVICE_TEST_DEFINES_H
