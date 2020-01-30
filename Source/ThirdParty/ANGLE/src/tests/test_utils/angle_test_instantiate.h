//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// angle_test_instantiate.h: Adds support for filtering parameterized
// tests by platform, so we skip unsupported configs.

#ifndef ANGLE_TEST_INSTANTIATE_H_
#define ANGLE_TEST_INSTANTIATE_H_

#include <gtest/gtest.h>

#include "common/platform.h"

namespace angle
{
struct SystemInfo;
struct PlatformParameters;

// Operating systems
bool IsAndroid();
bool IsLinux();
bool IsOSX();
bool IsOzone();
bool IsWindows();
bool IsWindows7();
bool IsFuchsia();

// Android devices
bool IsNexus5X();
bool IsNexus6P();
bool IsNexus9();
bool IsPixelXL();
bool IsPixel2();
bool IsPixel2XL();
bool IsNVIDIAShield();

// Desktop devices.
bool IsIntel();
bool IsAMD();
bool IsNVIDIA();

inline bool IsASan()
{
#if defined(ANGLE_WITH_ASAN)
    return true;
#else
    return false;
#endif  // defined(ANGLE_WITH_ASAN)
}

bool IsPlatformAvailable(const PlatformParameters &param);

// This functions is used to filter which tests should be registered,
// T must be or inherit from angle::PlatformParameters.
template <typename T>
std::vector<T> FilterTestParams(const T *params, size_t numParams)
{
    std::vector<T> filtered;

    for (size_t i = 0; i < numParams; i++)
    {
        if (IsPlatformAvailable(params[i]))
        {
            filtered.push_back(params[i]);
        }
    }

    return filtered;
}

template <typename T>
std::vector<T> FilterTestParams(const std::vector<T> &params)
{
    return FilterTestParams(params.data(), params.size());
}

// Used to generate valid test names out of testing::PrintToStringParamName used in combined tests.
struct CombinedPrintToStringParamName
{
    template <class ParamType>
    std::string operator()(const testing::TestParamInfo<ParamType> &info) const
    {
        std::string name = testing::PrintToStringParamName()(info);
        std::string sanitized;
        for (const char c : name)
        {
            if (c == ',')
            {
                sanitized += '_';
            }
            else if (isalnum(c) || c == '_')
            {
                sanitized += c;
            }
        }
        return sanitized;
    }
};

#define ANGLE_INSTANTIATE_TEST_PLATFORMS(testName, ...)                        \
    testing::ValuesIn(::angle::FilterTestParams(testName##__VA_ARGS__##params, \
                                                ArraySize(testName##__VA_ARGS__##params)))

// Instantiate the test once for each extra argument. The types of all the
// arguments must match, and getRenderer must be implemented for that type.
#define ANGLE_INSTANTIATE_TEST(testName, first, ...)                                 \
    const decltype(first) testName##params[] = {first, ##__VA_ARGS__};               \
    INSTANTIATE_TEST_SUITE_P(, testName, ANGLE_INSTANTIATE_TEST_PLATFORMS(testName), \
                             testing::PrintToStringParamName())

#define ANGLE_ALL_TEST_PLATFORMS_ES1 \
    ES1_D3D11(), ES1_OPENGL(), ES1_OPENGLES(), ES1_VULKAN(), ES1_VULKAN_SWIFTSHADER()

#define ANGLE_ALL_TEST_PLATFORMS_ES2                                                               \
    ES2_D3D9(), ES2_D3D11(), ES2_OPENGL(), ES2_OPENGLES(), ES2_VULKAN(), ES2_VULKAN_SWIFTSHADER(), \
        ES2_METAL()

#define ANGLE_ALL_TEST_PLATFORMS_ES3 \
    ES3_D3D11(), ES3_OPENGL(), ES3_OPENGLES(), ES3_VULKAN(), ES3_VULKAN_SWIFTSHADER()

#define ANGLE_ALL_TEST_PLATFORMS_ES31 \
    ES31_D3D11(), ES31_OPENGL(), ES31_OPENGLES(), ES31_VULKAN(), ES31_VULKAN_SWIFTSHADER()

#define ANGLE_ALL_TEST_PLATFORMS_NULL ES2_NULL(), ES3_NULL(), ES31_NULL()

// Instantiate the test once for each GLES1 platform
#define ANGLE_INSTANTIATE_TEST_ES1(testName)                                         \
    const PlatformParameters testName##params[] = {ANGLE_ALL_TEST_PLATFORMS_ES1};    \
    INSTANTIATE_TEST_SUITE_P(, testName, ANGLE_INSTANTIATE_TEST_PLATFORMS(testName), \
                             testing::PrintToStringParamName())

// Instantiate the test once for each GLES2 platform
#define ANGLE_INSTANTIATE_TEST_ES2(testName)                                         \
    const PlatformParameters testName##params[] = {ANGLE_ALL_TEST_PLATFORMS_ES2};    \
    INSTANTIATE_TEST_SUITE_P(, testName, ANGLE_INSTANTIATE_TEST_PLATFORMS(testName), \
                             testing::PrintToStringParamName())

// Instantiate the test once for each GLES3 platform
#define ANGLE_INSTANTIATE_TEST_ES3(testName)                                         \
    const PlatformParameters testName##params[] = {ANGLE_ALL_TEST_PLATFORMS_ES3};    \
    INSTANTIATE_TEST_SUITE_P(, testName, ANGLE_INSTANTIATE_TEST_PLATFORMS(testName), \
                             testing::PrintToStringParamName())

// Instantiate the test once for each GLES31 platform
#define ANGLE_INSTANTIATE_TEST_ES31(testName)                                        \
    const PlatformParameters testName##params[] = {ANGLE_ALL_TEST_PLATFORMS_ES31};   \
    INSTANTIATE_TEST_SUITE_P(, testName, ANGLE_INSTANTIATE_TEST_PLATFORMS(testName), \
                             testing::PrintToStringParamName())

// Multiple ES Version macros
#define ANGLE_INSTANTIATE_TEST_ES2_AND_ES3(testName)                                 \
    const PlatformParameters testName##params[] = {ANGLE_ALL_TEST_PLATFORMS_ES2,     \
                                                   ANGLE_ALL_TEST_PLATFORMS_ES3};    \
    INSTANTIATE_TEST_SUITE_P(, testName, ANGLE_INSTANTIATE_TEST_PLATFORMS(testName), \
                             testing::PrintToStringParamName())

#define ANGLE_INSTANTIATE_TEST_ES2_AND_ES3_AND_ES31(testName)                        \
    const PlatformParameters testName##params[] = {ANGLE_ALL_TEST_PLATFORMS_ES2,     \
                                                   ANGLE_ALL_TEST_PLATFORMS_ES3,     \
                                                   ANGLE_ALL_TEST_PLATFORMS_ES31};   \
    INSTANTIATE_TEST_SUITE_P(, testName, ANGLE_INSTANTIATE_TEST_PLATFORMS(testName), \
                             testing::PrintToStringParamName())

#define ANGLE_INSTANTIATE_TEST_ES2_AND_ES3_AND_ES31_AND_NULL(testName)                             \
    const PlatformParameters testName##params[] = {                                                \
        ANGLE_ALL_TEST_PLATFORMS_ES2, ANGLE_ALL_TEST_PLATFORMS_ES3, ANGLE_ALL_TEST_PLATFORMS_ES31, \
        ANGLE_ALL_TEST_PLATFORMS_NULL};                                                            \
    INSTANTIATE_TEST_SUITE_P(, testName, ANGLE_INSTANTIATE_TEST_PLATFORMS(testName),               \
                             testing::PrintToStringParamName())

#define ANGLE_INSTANTIATE_TEST_ES3_AND_ES31(testName)                                \
    const PlatformParameters testName##params[] = {ANGLE_ALL_TEST_PLATFORMS_ES3,     \
                                                   ANGLE_ALL_TEST_PLATFORMS_ES31};   \
    INSTANTIATE_TEST_SUITE_P(, testName, ANGLE_INSTANTIATE_TEST_PLATFORMS(testName), \
                             testing::PrintToStringParamName())

// Instantiate the test for a combination of N parameters and the enumeration of platforms in the
// extra args, similar to ANGLE_INSTANTIATE_TEST.  The macros are defined only for the Ns currently
// in use, and can be expanded as necessary.
#define ANGLE_INSTANTIATE_TEST_COMBINE_1(testName, print, combine1, first, ...) \
    const decltype(first) testName##params[] = {first, ##__VA_ARGS__};          \
    INSTANTIATE_TEST_SUITE_P(                                                   \
        , testName, testing::Combine(ANGLE_INSTANTIATE_TEST_PLATFORMS(testName), combine1), print)
#define ANGLE_INSTANTIATE_TEST_COMBINE_4(testName, print, combine1, combine2, combine3, combine4, \
                                         first, ...)                                              \
    const decltype(first) testName##params[] = {first, ##__VA_ARGS__};                            \
    INSTANTIATE_TEST_SUITE_P(, testName,                                                          \
                             testing::Combine(ANGLE_INSTANTIATE_TEST_PLATFORMS(testName),         \
                                              combine1, combine2, combine3, combine4),            \
                             print)
#define ANGLE_INSTANTIATE_TEST_COMBINE_5(testName, print, combine1, combine2, combine3, combine4, \
                                         combine5, first, ...)                                    \
    const decltype(first) testName##params[] = {first, ##__VA_ARGS__};                            \
    INSTANTIATE_TEST_SUITE_P(, testName,                                                          \
                             testing::Combine(ANGLE_INSTANTIATE_TEST_PLATFORMS(testName),         \
                                              combine1, combine2, combine3, combine4, combine5),  \
                             print)

// Checks if a config is expected to be supported by checking a system-based white list.
bool IsConfigWhitelisted(const SystemInfo &systemInfo, const PlatformParameters &param);

// Determines if a config is supported by trying to initialize it. Does not require SystemInfo.
bool IsConfigSupported(const PlatformParameters &param);

// Returns shared test system information. Can be used globally in the tests.
SystemInfo *GetTestSystemInfo();

// Returns a list of all enabled test platform names. For use in configuration enumeration.
std::vector<std::string> GetAvailableTestPlatformNames();

// Active config (e.g. ES2_Vulkan).
extern std::string gSelectedConfig;

// Use a separate isolated process per test config. This works around driver flakiness when using
// multiple APIs/windows/etc in the same process.
extern bool gSeparateProcessPerConfig;
}  // namespace angle

#define ANGLE_SKIP_TEST_IF(COND)                                  \
    do                                                            \
    {                                                             \
        if (COND)                                                 \
        {                                                         \
            std::cout << "Test skipped: " #COND "." << std::endl; \
            return;                                               \
        }                                                         \
    } while (0)

#endif  // ANGLE_TEST_INSTANTIATE_H_
