//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// angle_deqp_gtest:
//   dEQP and GoogleTest integration logic. Calls through to the random
//   order executor.

#include <stdint.h>
#include <array>
#include <fstream>

#include <gtest/gtest.h>

#include "angle_deqp_libtester.h"
#include "common/Optional.h"
#include "common/angleutils.h"
#include "common/debug.h"
#include "common/platform.h"
#include "common/string_utils.h"
#include "platform/Platform.h"
#include "tests/test_expectations/GPUTestConfig.h"
#include "tests/test_expectations/GPUTestExpectationsParser.h"
#include "util/system_utils.h"

namespace angle
{
namespace
{
bool gGlobalError = false;
bool gExpectError = false;

constexpr char kInfoTag[] = "*RESULT";

// Stored as globals to work around a Clang bug. http://crbug.com/951458
std::vector<std::string> gUnexpectedFailed;
std::vector<std::string> gUnexpectedPasses;

void HandlePlatformError(angle::PlatformMethods *platform, const char *errorMessage)
{
    if (!gExpectError)
    {
        FAIL() << errorMessage;
    }
    gGlobalError = true;
}

std::string DrawElementsToGoogleTestName(const std::string &dEQPName)
{
    std::string gTestName = dEQPName.substr(dEQPName.find('.') + 1);
    std::replace(gTestName.begin(), gTestName.end(), '.', '_');

    // Occurs in some luminance tests
    gTestName.erase(std::remove(gTestName.begin(), gTestName.end(), '-'), gTestName.end());
    return gTestName;
}

// We look for a GLES Khronos master list first. We keep the Android CTS so we can locate a version
// of egl-master.txt that has the full list of tests.
const char *gCaseListSearchPaths[] = {
    "/../../third_party/deqp/src/external/openglcts/data/mustpass/gles/aosp_mustpass/master/",
    "/../../third_party/angle/third_party/deqp/src/external/openglcts/data/mustpass/gles/"
    "aosp_mustpass/master/",
    "/../../sdcard/chromium_tests_root/third_party/angle/third_party/deqp/src/external/openglcts/"
    "data/mustpass/gles/aosp_mustpass/master/",
    "/../../third_party/deqp/src/android/cts/master/",
    "/../../third_party/angle/third_party/deqp/src/android/cts/master/",
    "/../../sdcard/chromium_tests_root/third_party/angle/third_party/deqp/src/android/cts/master/",
};

const char *gTestExpectationsSearchPaths[] = {
    "/../../src/tests/deqp_support/",
    "/../../third_party/angle/src/tests/deqp_support/",
    "/deqp_support/",
    "/../../sdcard/chromium_tests_root/third_party/angle/src/tests/deqp_support/",
};

const char *gCaseListFiles[] = {
    "gles2-master.txt",
    "gles3-master.txt",
    "gles31-master.txt",
    "egl-master.txt",
};

const char *gTestExpectationsFiles[] = {
    "deqp_gles2_test_expectations.txt",
    "deqp_gles3_test_expectations.txt",
    "deqp_gles31_test_expectations.txt",
    "deqp_egl_test_expectations.txt",
};

using APIInfo = std::pair<const char *, angle::GPUTestConfig::API>;

const APIInfo gEGLDisplayAPIs[] = {
    {"angle-d3d9", angle::GPUTestConfig::kAPID3D9},
    {"angle-d3d11", angle::GPUTestConfig::kAPID3D11},
    {"angle-gl", angle::GPUTestConfig::kAPIGLDesktop},
    {"angle-gles", angle::GPUTestConfig::kAPIGLES},
    {"angle-null", angle::GPUTestConfig::kAPIUnknown},
    {"angle-vulkan", angle::GPUTestConfig::kAPIVulkan},
};

const char *gdEQPEGLString  = "--deqp-egl-display-type=";
const char *gANGLEEGLString = "--use-angle=";
const char *gdEQPCaseString = "--deqp-case=";

std::array<char, 500> gCaseStringBuffer;

const APIInfo *gInitAPI = nullptr;

constexpr const char *gdEQPEGLConfigNameString = "--deqp-gl-config-name=";

// Default the config to RGBA8
const char *gEGLConfigName = "rgba8888d24s8";

// Returns the default API for a platform.
const char *GetDefaultAPIName()
{
#if defined(ANGLE_PLATFORM_WINDOWS)
    return "angle-d3d11";
#elif defined(ANGLE_PLATFORM_APPLE) || defined(ANGLE_PLATFORM_LINUX)
    return "angle-gl";
#elif defined(ANGLE_PLATFORM_ANDROID)
    return "angle-gles";
#else
#    error Unknown platform.
#endif
}

const APIInfo *FindAPIInfo(const std::string &arg)
{
    for (auto &displayAPI : gEGLDisplayAPIs)
    {
        if (arg == displayAPI.first)
        {
            return &displayAPI;
        }
    }
    return nullptr;
}

const APIInfo *GetDefaultAPIInfo()
{
    const APIInfo *defaultInfo = FindAPIInfo(GetDefaultAPIName());
    ASSERT(defaultInfo);
    return defaultInfo;
}

std::string GetTestStatLine(const std::string &key, const std::string &value)
{
    return std::string(kInfoTag) + ": " + key + ": " + value + "\n";
}

// During the CaseList initialization we cannot use the GTEST FAIL macro to quit the program because
// the initialization is called outside of tests the first time.
void Die()
{
    exit(EXIT_FAILURE);
}

Optional<std::string> FindFileFromPaths(const char *paths[],
                                        size_t numPaths,
                                        const std::string &exeDir,
                                        const std::string &searchFile)
{
    for (size_t pathIndex = 0; pathIndex < numPaths; ++pathIndex)
    {
        const char *testPath = paths[pathIndex];
        std::stringstream testExpectationsPathStr;
        testExpectationsPathStr << exeDir << testPath << searchFile;

        std::string path = testExpectationsPathStr.str();
        std::ifstream inFile(path.c_str());
        if (!inFile.fail())
        {
            inFile.close();
            return Optional<std::string>(path);
        }
    }

    return Optional<std::string>::Invalid();
}

Optional<std::string> FindCaseListPath(const std::string &exeDir, size_t testModuleIndex)
{
    return FindFileFromPaths(gCaseListSearchPaths, ArraySize(gCaseListSearchPaths), exeDir,
                             gCaseListFiles[testModuleIndex]);
}

Optional<std::string> FindTestExpectationsPath(const std::string &exeDir, size_t testModuleIndex)
{
    return FindFileFromPaths(gTestExpectationsSearchPaths, ArraySize(gTestExpectationsSearchPaths),
                             exeDir, gTestExpectationsFiles[testModuleIndex]);
}

class dEQPCaseList
{
  public:
    dEQPCaseList(size_t testModuleIndex);

    struct CaseInfo
    {
        CaseInfo(const std::string &dEQPName, const std::string &gTestName, int expectation)
            : mDEQPName(dEQPName), mGTestName(gTestName), mExpectation(expectation)
        {}

        std::string mDEQPName;
        std::string mGTestName;
        int mExpectation;
    };

    void initialize();

    const CaseInfo &getCaseInfo(size_t caseIndex) const
    {
        ASSERT(mInitialized);
        ASSERT(caseIndex < mCaseInfoList.size());
        return mCaseInfoList[caseIndex];
    }

    size_t numCases() const
    {
        ASSERT(mInitialized);
        return mCaseInfoList.size();
    }

  private:
    std::vector<CaseInfo> mCaseInfoList;
    angle::GPUTestExpectationsParser mTestExpectationsParser;
    size_t mTestModuleIndex;
    bool mInitialized = false;
};

dEQPCaseList::dEQPCaseList(size_t testModuleIndex) : mTestModuleIndex(testModuleIndex) {}

void dEQPCaseList::initialize()
{
    if (mInitialized)
    {
        return;
    }

    mInitialized = true;

    std::string exeDir = angle::GetExecutableDirectory();

    Optional<std::string> caseListPath = FindCaseListPath(exeDir, mTestModuleIndex);
    if (!caseListPath.valid())
    {
        std::cerr << "Failed to find case list file." << std::endl;
        Die();
    }

    Optional<std::string> testExpectationsPath = FindTestExpectationsPath(exeDir, mTestModuleIndex);
    if (!testExpectationsPath.valid())
    {
        std::cerr << "Failed to find test expectations file." << std::endl;
        Die();
    }

    angle::GPUTestConfig::API api = GetDefaultAPIInfo()->second;
    // Set the API from the command line, or using the default platform API.
    if (gInitAPI)
    {
        api = gInitAPI->second;
    }

    angle::GPUTestConfig testConfig = angle::GPUTestConfig(api);

    if (!mTestExpectationsParser.loadTestExpectationsFromFile(testConfig,
                                                              testExpectationsPath.value()))
    {
        std::stringstream errorMsgStream;
        for (const auto &message : mTestExpectationsParser.getErrorMessages())
        {
            errorMsgStream << std::endl << " " << message;
        }

        std::cerr << "Failed to load test expectations." << errorMsgStream.str() << std::endl;
        Die();
    }

    std::ifstream caseListStream(caseListPath.value());
    if (caseListStream.fail())
    {
        std::cerr << "Failed to load the case list." << std::endl;
        Die();
    }

    while (!caseListStream.eof())
    {
        std::string inString;
        std::getline(caseListStream, inString);

        std::string dEQPName = angle::TrimString(inString, angle::kWhitespaceASCII);
        if (dEQPName.empty())
            continue;
        std::string gTestName = DrawElementsToGoogleTestName(dEQPName);
        if (gTestName.empty())
            continue;

        int expectation = mTestExpectationsParser.getTestExpectation(dEQPName);
        mCaseInfoList.push_back(CaseInfo(dEQPName, gTestName, expectation));
    }

    std::stringstream unusedMsgStream;
    bool anyUnused = false;
    for (const auto &message : mTestExpectationsParser.getUnusedExpectationsMessages())
    {
        anyUnused = true;
        unusedMsgStream << std::endl << " " << message;
    }
    if (anyUnused)
    {
        std::cerr << "Failed to validate test expectations." << unusedMsgStream.str() << std::endl;
        Die();
    }
}

template <size_t TestModuleIndex>
class dEQPTest : public testing::TestWithParam<size_t>
{
  public:
    static testing::internal::ParamGenerator<size_t> GetTestingRange()
    {
        return testing::Range<size_t>(0, GetCaseList().numCases());
    }

    static std::string GetCaseGTestName(size_t caseIndex)
    {
        const auto &caseInfo = GetCaseList().getCaseInfo(caseIndex);
        return caseInfo.mGTestName;
    }

    static const dEQPCaseList &GetCaseList()
    {
        static dEQPCaseList sCaseList(TestModuleIndex);
        sCaseList.initialize();
        return sCaseList;
    }

    static void SetUpTestCase();
    static void TearDownTestCase();

  protected:
    void runTest() const
    {
        if (sTestExceptionCount > 1)
        {
            std::cout << "Too many exceptions, skipping all remaining tests." << std::endl;
            return;
        }

        const auto &caseInfo = GetCaseList().getCaseInfo(GetParam());
        std::cout << caseInfo.mDEQPName << std::endl;

        // Tests that crash exit the harness before collecting the result. To tally the number of
        // crashed tests we track how many tests we "tried" to run.
        sTestCount++;

        if (caseInfo.mExpectation == angle::GPUTestExpectationsParser::kGpuTestSkip)
        {
            sSkippedTestCount++;
            std::cout << "Test skipped.\n";
            return;
        }

        gExpectError = (caseInfo.mExpectation != angle::GPUTestExpectationsParser::kGpuTestPass);
        TestResult result = deqp_libtester_run(caseInfo.mDEQPName.c_str());

        bool testSucceeded = countTestResultAndReturnSuccess(result);

        // Check the global error flag for unexpected platform errors.
        if (gGlobalError)
        {
            testSucceeded = false;
            gGlobalError  = false;
        }

        if (caseInfo.mExpectation == angle::GPUTestExpectationsParser::kGpuTestPass)
        {
            EXPECT_TRUE(testSucceeded);

            if (!testSucceeded)
            {
                gUnexpectedFailed.push_back(caseInfo.mDEQPName);
            }
        }
        else if (testSucceeded)
        {
            std::cout << "Test expected to fail but passed!" << std::endl;
            gUnexpectedPasses.push_back(caseInfo.mDEQPName);
        }
    }

    bool countTestResultAndReturnSuccess(TestResult result) const
    {
        switch (result)
        {
            case TestResult::Pass:
                sPassedTestCount++;
                return true;
            case TestResult::Fail:
                sFailedTestCount++;
                return false;
            case TestResult::NotSupported:
                sNotSupportedTestCount++;
                return true;
            case TestResult::Exception:
                sTestExceptionCount++;
                return false;
            default:
                std::cerr << "Unexpected test result code: " << static_cast<int>(result) << "\n";
                return false;
        }
    }

    static void PrintTestStats()
    {
        uint32_t crashedCount =
            sTestCount - (sPassedTestCount + sFailedTestCount + sNotSupportedTestCount +
                          sTestExceptionCount + sSkippedTestCount);

        std::cout << GetTestStatLine("Total", std::to_string(sTestCount));
        std::cout << GetTestStatLine("Passed", std::to_string(sPassedTestCount));
        std::cout << GetTestStatLine("Failed", std::to_string(sFailedTestCount));
        std::cout << GetTestStatLine("Skipped", std::to_string(sSkippedTestCount));
        std::cout << GetTestStatLine("Not Supported", std::to_string(sNotSupportedTestCount));
        std::cout << GetTestStatLine("Exception", std::to_string(sTestExceptionCount));
        std::cout << GetTestStatLine("Crashed", std::to_string(crashedCount));

        if (!gUnexpectedPasses.empty())
        {
            std::cout << GetTestStatLine("Unexpected Passed Count",
                                         std::to_string(gUnexpectedPasses.size()));
            for (const std::string &testName : gUnexpectedPasses)
            {
                std::cout << GetTestStatLine("Unexpected Passed Tests", testName);
            }
        }

        if (!gUnexpectedFailed.empty())
        {
            std::cout << GetTestStatLine("Unexpected Failed Count",
                                         std::to_string(gUnexpectedFailed.size()));
            for (const std::string &testName : gUnexpectedFailed)
            {
                std::cout << GetTestStatLine("Unexpected Failed Tests", testName);
            }
        }
    }

    static uint32_t sTestCount;
    static uint32_t sPassedTestCount;
    static uint32_t sFailedTestCount;
    static uint32_t sTestExceptionCount;
    static uint32_t sNotSupportedTestCount;
    static uint32_t sSkippedTestCount;
};

template <size_t TestModuleIndex>
uint32_t dEQPTest<TestModuleIndex>::sTestCount = 0;
template <size_t TestModuleIndex>
uint32_t dEQPTest<TestModuleIndex>::sPassedTestCount = 0;
template <size_t TestModuleIndex>
uint32_t dEQPTest<TestModuleIndex>::sFailedTestCount = 0;
template <size_t TestModuleIndex>
uint32_t dEQPTest<TestModuleIndex>::sTestExceptionCount = 0;
template <size_t TestModuleIndex>
uint32_t dEQPTest<TestModuleIndex>::sNotSupportedTestCount = 0;
template <size_t TestModuleIndex>
uint32_t dEQPTest<TestModuleIndex>::sSkippedTestCount = 0;

// static
template <size_t TestModuleIndex>
void dEQPTest<TestModuleIndex>::SetUpTestCase()
{
    sPassedTestCount       = 0;
    sFailedTestCount       = 0;
    sNotSupportedTestCount = 0;
    sTestExceptionCount    = 0;
    sTestCount             = 0;
    sSkippedTestCount      = 0;
    gUnexpectedPasses.clear();
    gUnexpectedFailed.clear();

    std::vector<const char *> argv;

    // Reserve one argument for the binary name.
    argv.push_back("");

    // Add init api.
    const char *targetApi    = gInitAPI ? gInitAPI->first : GetDefaultAPIName();
    std::string apiArgString = std::string(gdEQPEGLString) + targetApi;
    argv.push_back(apiArgString.c_str());

    // Add config name
    const char *targetConfigName = gEGLConfigName;
    std::string configArgString  = std::string(gdEQPEGLConfigNameString) + targetConfigName;
    argv.push_back(configArgString.c_str());

    // Init the platform.
    if (!deqp_libtester_init_platform(static_cast<int>(argv.size()), argv.data(),
                                      reinterpret_cast<void *>(&HandlePlatformError)))
    {
        std::cout << "Aborting test due to dEQP initialization error." << std::endl;
        exit(1);
    }
}

// static
template <size_t TestModuleIndex>
void dEQPTest<TestModuleIndex>::TearDownTestCase()
{
    PrintTestStats();
    deqp_libtester_shutdown_platform();
}

#define ANGLE_INSTANTIATE_DEQP_TEST_CASE(API, N)                              \
    class dEQP : public dEQPTest<N>                                           \
    {};                                                                       \
    TEST_P(dEQP, API) { runTest(); }                                          \
                                                                              \
    INSTANTIATE_TEST_SUITE_P(, dEQP, dEQP::GetTestingRange(),                 \
                             [](const testing::TestParamInfo<size_t> &info) { \
                                 return dEQP::GetCaseGTestName(info.param);   \
                             })

#ifdef ANGLE_DEQP_GLES2_TESTS
ANGLE_INSTANTIATE_DEQP_TEST_CASE(GLES2, 0);
#endif

#ifdef ANGLE_DEQP_GLES3_TESTS
ANGLE_INSTANTIATE_DEQP_TEST_CASE(GLES3, 1);
#endif

#ifdef ANGLE_DEQP_GLES31_TESTS
ANGLE_INSTANTIATE_DEQP_TEST_CASE(GLES31, 2);
#endif

#ifdef ANGLE_DEQP_EGL_TESTS
ANGLE_INSTANTIATE_DEQP_TEST_CASE(EGL, 3);
#endif

void HandleDisplayType(const char *displayTypeString)
{
    std::stringstream argStream;

    if (gInitAPI)
    {
        std::cout << "Cannot specify two EGL displays!" << std::endl;
        exit(1);
    }

    if (strncmp(displayTypeString, "angle-", strlen("angle-")) != 0)
    {
        argStream << "angle-";
    }

    argStream << displayTypeString;
    std::string arg = argStream.str();

    gInitAPI = FindAPIInfo(arg);

    if (!gInitAPI)
    {
        std::cout << "Unknown ANGLE back-end API: " << displayTypeString << std::endl;
        exit(1);
    }
}

void HandleEGLConfigName(const char *configNameString)
{
    gEGLConfigName = configNameString;
}

// The --deqp-case flag takes a case expression that is parsed into a --gtest_filter. It converts
// the "dEQP" style names (functional.thing.*) into "GoogleTest" style names (functional_thing_*).
// Currently it does not handle multiple tests and multiple filters in different arguments.
void HandleCaseName(const char *caseString, int *argc, int argIndex, char **argv)
{
    std::string googleTestName = DrawElementsToGoogleTestName(caseString);
    gCaseStringBuffer.fill(0);
    int bytesWritten = snprintf(gCaseStringBuffer.data(), gCaseStringBuffer.size() - 1,
                                "--gtest_filter=*%s", googleTestName.c_str());
    if (bytesWritten <= 0 || static_cast<size_t>(bytesWritten) >= gCaseStringBuffer.size() - 1)
    {
        std::cout << "Error parsing test case string: " << caseString;
        exit(1);
    }

    argv[argIndex] = gCaseStringBuffer.data();
}

void DeleteArg(int *argc, int argIndex, char **argv)
{
    (*argc)--;
    for (int moveIndex = argIndex; moveIndex < *argc; ++moveIndex)
    {
        argv[moveIndex] = argv[moveIndex + 1];
    }
}
}  // anonymous namespace

// Called from main() to process command-line arguments.
void InitTestHarness(int *argc, char **argv)
{
    int argIndex = 0;
    while (argIndex < *argc)
    {
        if (strncmp(argv[argIndex], gdEQPEGLString, strlen(gdEQPEGLString)) == 0)
        {
            HandleDisplayType(argv[argIndex] + strlen(gdEQPEGLString));
            DeleteArg(argc, argIndex, argv);
        }
        else if (strncmp(argv[argIndex], gANGLEEGLString, strlen(gANGLEEGLString)) == 0)
        {
            HandleDisplayType(argv[argIndex] + strlen(gANGLEEGLString));
            DeleteArg(argc, argIndex, argv);
        }
        else if (strncmp(argv[argIndex], gdEQPEGLConfigNameString,
                         strlen(gdEQPEGLConfigNameString)) == 0)
        {
            HandleEGLConfigName(argv[argIndex] + strlen(gdEQPEGLConfigNameString));
            DeleteArg(argc, argIndex, argv);
        }
        else if (strncmp(argv[argIndex], gdEQPCaseString, strlen(gdEQPCaseString)) == 0)
        {
            HandleCaseName(argv[argIndex] + strlen(gdEQPCaseString), argc, argIndex, argv);
            argIndex++;
        }
        else
        {
            argIndex++;
        }
    }
}
}  // namespace angle
