/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_VOE_TEST_DEFINES_H
#define WEBRTC_VOICE_ENGINE_VOE_TEST_DEFINES_H

#include "webrtc/voice_engine/test/auto_test/voe_test_common.h"

// Select the tests to execute, list order below is same as they will be
// executed. Note that, all settings below will be overriden by sub-API
// settings in voice_engine_configurations.h.
#define _TEST_BASE_
#define _TEST_RTP_RTCP_
#define _TEST_CODEC_
#define _TEST_FILE_
#define _TEST_NETWORK_

// Enable this when running instrumentation of some kind to exclude tests
// that will not pass due to slowed down execution.
// #define _INSTRUMENTATION_TESTING_

// Some parts can cause problems while running Insure
#ifdef __INSURE__
#define _INSTRUMENTATION_TESTING_
#endif

#define MARK() TEST_LOG("."); fflush(NULL);             // Add test marker
#define ANL() TEST_LOG("\n")                            // Add New Line
#define AOK() TEST_LOG("[Test is OK]"); fflush(NULL);   // Add OK
#if defined(_WIN32)
#define PAUSE                                      \
    {                                               \
        TEST_LOG("Press any key to continue...");   \
        _getch();                                   \
        TEST_LOG("\n");                             \
    }
#else
#define PAUSE                                          \
    {                                                   \
        TEST_LOG("Continuing (pause not supported)\n"); \
    }
#endif

#define TEST(s)                         \
    {                                   \
        TEST_LOG("Testing: %s", #s);    \
    }                                   \

#ifdef _INSTRUMENTATION_TESTING_
// Don't stop execution if error occurs
#define TEST_MUSTPASS(expr)                                               \
    {                                                                     \
        if ((expr))                                                       \
        {                                                                 \
            TEST_LOG_ERROR("Error at line:%i, %s \n",__LINE__, #expr);    \
            TEST_LOG_ERROR("Error code: %i\n",voe_base_->LastError());    \
        }                                                                 \
    }
#define TEST_ERROR(code)                                                \
    {                                                                   \
        int err = voe_base_->LastError();                               \
        if (err != code)                                                \
        {                                                               \
            TEST_LOG_ERROR("Invalid error code (%d, should be %d) at line %d\n",
                           code, err, __LINE__);
}
}
#else
#define ASSERT_TRUE(expr) TEST_MUSTPASS(!(expr))
#define ASSERT_FALSE(expr) TEST_MUSTPASS(expr)
#define TEST_MUSTFAIL(expr) TEST_MUSTPASS(!((expr) == -1))
#define TEST_MUSTPASS(expr)                                              \
    {                                                                    \
        if ((expr))                                                      \
        {                                                                \
            TEST_LOG_ERROR("\nError at line:%i, %s \n",__LINE__, #expr); \
            TEST_LOG_ERROR("Error code: %i\n", voe_base_->LastError());  \
            PAUSE                                                        \
            return -1;                                                   \
        }                                                                \
    }
#define TEST_ERROR(code) \
    {																                                         \
      int err = voe_base_->LastError();                                      \
      if (err != code)                                                       \
      {                                                                      \
        TEST_LOG_ERROR("Invalid error code (%d, should be %d) at line %d\n", \
                       err, code, __LINE__);                                 \
        PAUSE                                                                \
        return -1;                                                           \
      }															                                         \
    }
#endif  // #ifdef _INSTRUMENTATION_TESTING_
#define EXCLUDE()                                                   \
    {                                                               \
        TEST_LOG("\n>>> Excluding test at line: %i <<<\n\n",__LINE__);  \
    }

#define INCOMPLETE()                                                \
    {                                                               \
        TEST_LOG("\n>>> Incomplete test at line: %i <<<\n\n",__LINE__);  \
    }

#endif // WEBRTC_VOICE_ENGINE_VOE_TEST_DEFINES_H
