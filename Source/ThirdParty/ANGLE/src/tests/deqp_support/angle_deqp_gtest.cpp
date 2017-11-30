//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// angle_deqp_gtest:
//   dEQP and GoogleTest integration logic. Calls through to the random
//   order executor.

#include <fstream>
#include <stdint.h>

#include <gtest/gtest.h>

#include "angle_deqp_libtester.h"
#include "common/angleutils.h"
#include "common/debug.h"
#include "common/Optional.h"
#include "common/platform.h"
#include "common/string_utils.h"
#include "gpu_test_expectations_parser.h"
#include "system_utils.h"

namespace
{

#if defined(ANGLE_PLATFORM_ANDROID)
const char *g_CaseListRelativePath =
    "/../../sdcard/chromium_tests_root/third_party/deqp/src/android/cts/master/";
#else
const char *g_CaseListRelativePath = "/../../third_party/deqp/src/android/cts/master/";
#endif

const char *g_TestExpectationsSearchPaths[] = {
    "/../../src/tests/deqp_support/", "/../../third_party/angle/src/tests/deqp_support/",
    "/deqp_support/", "/../../sdcard/chromium_tests_root/third_party/angle/src/tests/deqp_support/",
};

const char *g_CaseListFiles[] = {
    "gles2-master.txt", "gles3-master.txt", "gles31-master.txt", "egl-master.txt",
};

const char *g_TestExpectationsFiles[] = {
    "deqp_gles2_test_expectations.txt", "deqp_gles3_test_expectations.txt",
    "deqp_gles31_test_expectations.txt", "deqp_egl_test_expectations.txt",
};

using APIInfo = std::pair<const char *, gpu::GPUTestConfig::API>;

const APIInfo g_eglDisplayAPIs[] = {
    {"angle-d3d9", gpu::GPUTestConfig::kAPID3D9},
    {"angle-d3d11", gpu::GPUTestConfig::kAPID3D11},
    {"angle-gl", gpu::GPUTestConfig::kAPIGLDesktop},
    {"angle-gles", gpu::GPUTestConfig::kAPIGLES},
    {"angle-null", gpu::GPUTestConfig::kAPIUnknown },
};

const char *g_deqpEGLString = "--deqp-egl-display-type=";
const char *g_angleEGLString = "--use-angle=";

const APIInfo *g_initAPI = nullptr;

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
#error Unknown platform.
#endif
}

const APIInfo *FindAPIInfo(const std::string &arg)
{
    for (auto &displayAPI : g_eglDisplayAPIs)
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

// During the CaseList initialization we cannot use the GTEST FAIL macro to quit the program because
// the initialization is called outside of tests the first time.
void Die()
{
    exit(EXIT_FAILURE);
}

Optional<std::string> FindTestExpectationsPath(const std::string &exeDir,
                                                      size_t testModuleIndex)
{
    for (const char *testPath : g_TestExpectationsSearchPaths)
    {
        std::stringstream testExpectationsPathStr;
        testExpectationsPathStr << exeDir << testPath << g_TestExpectationsFiles[testModuleIndex];

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

class dEQPCaseList
{
  public:
    dEQPCaseList(size_t testModuleIndex);

    struct CaseInfo
    {
        CaseInfo(const std::string &dEQPName,
                 const std::string &gTestName,
                 int expectation)
            : mDEQPName(dEQPName),
              mGTestName(gTestName),
              mExpectation(expectation)
        {
        }

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
    gpu::GPUTestExpectationsParser mTestExpectationsParser;
    gpu::GPUTestBotConfig mTestConfig;
    size_t mTestModuleIndex;
    bool mInitialized;
};

dEQPCaseList::dEQPCaseList(size_t testModuleIndex)
    : mTestModuleIndex(testModuleIndex),
      mInitialized(false)
{
}

void dEQPCaseList::initialize()
{
    if (mInitialized)
    {
        return;
    }

    mInitialized = true;

    std::string exeDir = angle::GetExecutableDirectory();

    std::stringstream caseListPathStr;
    caseListPathStr << exeDir << g_CaseListRelativePath << g_CaseListFiles[mTestModuleIndex];
    std::string caseListPath = caseListPathStr.str();

    Optional<std::string> testExpectationsPath = FindTestExpectationsPath(exeDir, mTestModuleIndex);
    if (!testExpectationsPath.valid())
    {
        std::cerr << "Failed to find test expectations file." << std::endl;
        Die();
    }

    if (!mTestExpectationsParser.LoadTestExpectationsFromFile(testExpectationsPath.value()))
    {
        std::stringstream errorMsgStream;
        for (const auto &message : mTestExpectationsParser.GetErrorMessages())
        {
            errorMsgStream << std::endl << " " << message;
        }

        std::cerr << "Failed to load test expectations." << errorMsgStream.str() << std::endl;
        Die();
    }

    if (!mTestConfig.LoadCurrentConfig(nullptr))
    {
        std::cerr << "Failed to load test configuration." << std::endl;
        Die();
    }

    // Set the API from the command line, or using the default platform API.
    if (g_initAPI)
    {
        mTestConfig.set_api(g_initAPI->second);
    }
    else
    {
        mTestConfig.set_api(GetDefaultAPIInfo()->second);
    }

    std::ifstream caseListStream(caseListPath);
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
        std::string gTestName = dEQPName.substr(dEQPName.find('.') + 1);
        if (gTestName.empty())
            continue;
        std::replace(gTestName.begin(), gTestName.end(), '.', '_');

        // Occurs in some luminance tests
        gTestName.erase(std::remove(gTestName.begin(), gTestName.end(), '-'), gTestName.end());

        int expectation = mTestExpectationsParser.GetTestExpectation(dEQPName, mTestConfig);
        if (expectation != gpu::GPUTestExpectationsParser::kGpuTestSkip)
        {
            mCaseInfoList.push_back(CaseInfo(dEQPName, gTestName, expectation));
        }
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
    void runTest()
    {
        const auto &caseInfo = GetCaseList().getCaseInfo(GetParam());
        std::cout << caseInfo.mDEQPName << std::endl;

        bool result = deqp_libtester_run(caseInfo.mDEQPName.c_str());

        if (caseInfo.mExpectation == gpu::GPUTestExpectationsParser::kGpuTestPass)
        {
            EXPECT_TRUE(result);
            sPasses += (result ? 1u : 0u);
            sFails += (!result ? 1u : 0u);
        }
        else if (result)
        {
            std::cout << "Test expected to fail but passed!" << std::endl;
            sUnexpectedPasses++;
        }
        else
        {
            sFails++;
        }
    }

    static unsigned int sPasses;
    static unsigned int sFails;
    static unsigned int sUnexpectedPasses;
};

template <size_t TestModuleIndex>
unsigned int dEQPTest<TestModuleIndex>::sPasses           = 0;
template <size_t TestModuleIndex>
unsigned int dEQPTest<TestModuleIndex>::sFails            = 0;
template <size_t TestModuleIndex>
unsigned int dEQPTest<TestModuleIndex>::sUnexpectedPasses = 0;

// static
template <size_t TestModuleIndex>
void dEQPTest<TestModuleIndex>::SetUpTestCase()
{
    sPasses           = 0;
    sFails            = 0;
    sUnexpectedPasses = 0;

    std::vector<const char *> argv;

    // Reserve one argument for the binary name.
    argv.push_back("");

    // Add init api.
    const char *targetApi    = g_initAPI ? g_initAPI->first : GetDefaultAPIName();
    std::string apiArgString = std::string(g_deqpEGLString) + targetApi;
    argv.push_back(apiArgString.c_str());

    // Init the platform.
    if (!deqp_libtester_init_platform(static_cast<int>(argv.size()), argv.data()))
    {
        std::cout << "Aborting test due to dEQP initialization error." << std::endl;
        exit(1);
    }
}

// static
template <size_t TestModuleIndex>
void dEQPTest<TestModuleIndex>::TearDownTestCase()
{
    unsigned int total = sPasses + sFails;
    float passFrac     = static_cast<float>(sPasses) / static_cast<float>(total) * 100.0f;
    float failFrac     = static_cast<float>(sFails) / static_cast<float>(total) * 100.0f;
    std::cout << "Passed: " << sPasses << "/" << total << " tests. (" << passFrac << "%)"
              << std::endl;
    if (sFails > 0)
    {
        std::cout << "Failed: " << sFails << "/" << total << " tests. (" << failFrac << "%)"
                  << std::endl;
    }
    if (sUnexpectedPasses > 0)
    {
        std::cout << sUnexpectedPasses << " tests unexpectedly passed." << std::endl;
    }

    deqp_libtester_shutdown_platform();
}

#define ANGLE_INSTANTIATE_DEQP_TEST_CASE(DEQP_TEST, N)                          \
    class DEQP_TEST : public dEQPTest<N>                                        \
    {                                                                           \
    };                                                                          \
    TEST_P(DEQP_TEST, Default) { runTest(); }                                   \
                                                                                \
    INSTANTIATE_TEST_CASE_P(, DEQP_TEST, DEQP_TEST::GetTestingRange(),          \
                            [](const testing::TestParamInfo<size_t> &info) {    \
                                return DEQP_TEST::GetCaseGTestName(info.param); \
                            })

#ifdef ANGLE_DEQP_GLES2_TESTS
ANGLE_INSTANTIATE_DEQP_TEST_CASE(dEQP_GLES2, 0);
#endif

#ifdef ANGLE_DEQP_GLES3_TESTS
ANGLE_INSTANTIATE_DEQP_TEST_CASE(dEQP_GLES3, 1);
#endif

#ifdef ANGLE_DEQP_GLES31_TESTS
ANGLE_INSTANTIATE_DEQP_TEST_CASE(dEQP_GLES31, 2);
#endif

#ifdef ANGLE_DEQP_EGL_TESTS
ANGLE_INSTANTIATE_DEQP_TEST_CASE(dEQP_EGL, 3);
#endif

void HandleDisplayType(const char *displayTypeString)
{
    std::stringstream argStream;

    if (g_initAPI)
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

    g_initAPI = FindAPIInfo(arg);

    if (!g_initAPI)
    {
        std::cout << "Unknown ANGLE back-end API: " << displayTypeString << std::endl;
        exit(1);
    }
}

void DeleteArg(int *argc, int argIndex, char **argv)
{
    (*argc)--;
    for (int moveIndex = argIndex; moveIndex < *argc; ++moveIndex)
    {
        argv[moveIndex] = argv[moveIndex + 1];
    }
}

} // anonymous namespace

// Called from main() to process command-line arguments.
namespace angle
{
void InitTestHarness(int *argc, char **argv)
{
    int argIndex = 0;
    while (argIndex < *argc)
    {
        if (strncmp(argv[argIndex], g_deqpEGLString, strlen(g_deqpEGLString)) == 0)
        {
            HandleDisplayType(argv[argIndex] + strlen(g_deqpEGLString));
            DeleteArg(argc, argIndex, argv);
        }
        else if (strncmp(argv[argIndex], g_angleEGLString, strlen(g_angleEGLString)) == 0)
        {
            HandleDisplayType(argv[argIndex] + strlen(g_angleEGLString));
            DeleteArg(argc, argIndex, argv);
        }
        else
        {
            argIndex++;
        }
    }
}
}  // namespace angle
