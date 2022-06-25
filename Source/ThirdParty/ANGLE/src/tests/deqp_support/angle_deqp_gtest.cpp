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
#include "common/system_utils.h"
#include "platform/PlatformMethods.h"
#include "tests/test_utils/runner/TestSuite.h"
#include "util/OSWindow.h"
#include "util/test_utils.h"

namespace angle
{
namespace
{
#if !defined(NDEBUG)
constexpr bool kIsDebug = true;
#else
constexpr bool kIsDebug                = false;
#endif  // !defined(NDEBUG)

bool gGlobalError = false;
bool gExpectError = false;
bool gVerbose     = false;

// Set this to true temporarily to enable image logging in release. Useful for diagnosing errors.
bool gLogImages = kIsDebug;

constexpr char kInfoTag[] = "*RESULT";

void HandlePlatformError(PlatformMethods *platform, const char *errorMessage)
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

// Relative to the ANGLE root folder.
constexpr char kCTSRootPath[] = "third_party/VK-GL-CTS/src/";
constexpr char kSupportPath[] = "src/tests/deqp_support/";

#define OPENGL_CTS_DIR(PATH) "external/openglcts/data/mustpass/gles/" PATH

const char *gCaseListFiles[] = {
    OPENGL_CTS_DIR("aosp_mustpass/main/gles2-master.txt"),
    OPENGL_CTS_DIR("aosp_mustpass/main/gles3-master.txt"),
    OPENGL_CTS_DIR("aosp_mustpass/main/gles31-master.txt"),
    "/android/cts/main/egl-master.txt",
    OPENGL_CTS_DIR("khronos_mustpass/main/gles2-khr-master.txt"),
    OPENGL_CTS_DIR("khronos_mustpass/main/gles3-khr-master.txt"),
    OPENGL_CTS_DIR("khronos_mustpass/main/gles31-khr-master.txt"),
    OPENGL_CTS_DIR("khronos_mustpass/main/gles32-khr-master.txt"),
    OPENGL_CTS_DIR("aosp_mustpass/main/gles3-rotate-landscape.txt"),
    OPENGL_CTS_DIR("aosp_mustpass/main/gles3-rotate-reverse-portrait.txt"),
    OPENGL_CTS_DIR("aosp_mustpass/main/gles3-rotate-reverse-landscape.txt"),
    OPENGL_CTS_DIR("aosp_mustpass/main/gles31-rotate-landscape.txt"),
    OPENGL_CTS_DIR("aosp_mustpass/main/gles31-rotate-reverse-portrait.txt"),
    OPENGL_CTS_DIR("aosp_mustpass/main/gles31-rotate-reverse-landscape.txt"),
};

#undef OPENGL_CTS_DIR

const char *gTestExpectationsFiles[] = {
    "deqp_gles2_test_expectations.txt",         "deqp_gles3_test_expectations.txt",
    "deqp_gles31_test_expectations.txt",        "deqp_egl_test_expectations.txt",
    "deqp_khr_gles2_test_expectations.txt",     "deqp_khr_gles3_test_expectations.txt",
    "deqp_khr_gles31_test_expectations.txt",    "deqp_khr_gles32_test_expectations.txt",
    "deqp_gles3_rotate_test_expectations.txt",  "deqp_gles3_rotate_test_expectations.txt",
    "deqp_gles3_rotate_test_expectations.txt",  "deqp_gles31_rotate_test_expectations.txt",
    "deqp_gles31_rotate_test_expectations.txt", "deqp_gles31_rotate_test_expectations.txt",
};

using APIInfo = std::pair<const char *, GPUTestConfig::API>;

constexpr APIInfo kEGLDisplayAPIs[] = {
    {"angle-d3d9", GPUTestConfig::kAPID3D9},
    {"angle-d3d11", GPUTestConfig::kAPID3D11},
    {"angle-d3d11-ref", GPUTestConfig::kAPID3D11},
    {"angle-gl", GPUTestConfig::kAPIGLDesktop},
    {"angle-gles", GPUTestConfig::kAPIGLES},
    {"angle-metal", GPUTestConfig::kAPIMetal},
    {"angle-null", GPUTestConfig::kAPIUnknown},
    {"angle-swiftshader", GPUTestConfig::kAPISwiftShader},
    {"angle-vulkan", GPUTestConfig::kAPIVulkan},
};

constexpr char kdEQPEGLString[]     = "--deqp-egl-display-type=";
constexpr char kANGLEEGLString[]    = "--use-angle=";
constexpr char kANGLEPreRotation[]  = "--emulated-pre-rotation=";
constexpr char kdEQPCaseString[]    = "--deqp-case=";
constexpr char kVerboseString[]     = "--verbose";
constexpr char kRenderDocString[]   = "--renderdoc";
constexpr char kNoRenderDocString[] = "--no-renderdoc";
constexpr char kdEQPFlagsPrefix[]   = "--deqp-";

std::array<char, 500> gCaseStringBuffer;

// For angle_deqp_gles3*_rotateN_tests, default gOptions.preRotation to N.
#if defined(ANGLE_DEQP_GLES3_ROTATE90_TESTS) || defined(ANGLE_DEQP_GLES31_ROTATE90_TESTS)
constexpr uint32_t kDefaultPreRotation = 90;
#elif defined(ANGLE_DEQP_GLES3_ROTATE180_TESTS) || defined(ANGLE_DEQP_GLES31_ROTATE180_TESTS)
constexpr uint32_t kDefaultPreRotation = 180;
#elif defined(ANGLE_DEQP_GLES3_ROTATE270_TESTS) || defined(ANGLE_DEQP_GLES31_ROTATE270_TESTS)
constexpr uint32_t kDefaultPreRotation = 270;
#else
constexpr uint32_t kDefaultPreRotation = 0;
#endif

#if defined(ANGLE_TEST_ENABLE_RENDERDOC_CAPTURE)
constexpr bool kEnableRenderDocCapture = true;
#else
constexpr bool kEnableRenderDocCapture = false;
#endif

const APIInfo *gInitAPI = nullptr;
dEQPOptions gOptions    = {
    kDefaultPreRotation,      // preRotation
    kEnableRenderDocCapture,  // enableRenderDocCapture
};

constexpr const char gdEQPEGLConfigNameString[] = "--deqp-gl-config-name=";
constexpr const char gdEQPLogImagesString[]     = "--deqp-log-images=";

// Default the config to RGBA8
const char *gEGLConfigName = "rgba8888d24s8";

std::vector<const char *> gdEQPForwardFlags;

// Returns the default API for a platform.
const char *GetDefaultAPIName()
{
#if defined(ANGLE_PLATFORM_ANDROID) || defined(ANGLE_PLATFORM_LINUX) || \
    defined(ANGLE_PLATFORM_WINDOWS)
    return "angle-vulkan";
#elif defined(ANGLE_PLATFORM_APPLE)
    return "angle-gl";
#else
#    error Unknown platform.
#endif
}

const APIInfo *FindAPIInfo(const std::string &arg)
{
    for (auto &displayAPI : kEGLDisplayAPIs)
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

Optional<std::string> FindFileFromPath(const char *dirPath, const char *filePath)
{
    std::stringstream strstr;
    strstr << dirPath << filePath;
    std::string path = strstr.str();

    constexpr size_t kMaxFoundPathLen = 1000;
    char foundPath[kMaxFoundPathLen];
    if (angle::FindTestDataPath(path.c_str(), foundPath, kMaxFoundPathLen))
    {
        return std::string(foundPath);
    }

    return Optional<std::string>::Invalid();
}

Optional<std::string> FindCaseListPath(size_t testModuleIndex)
{
    return FindFileFromPath(kCTSRootPath, gCaseListFiles[testModuleIndex]);
}

Optional<std::string> FindTestExpectationsPath(size_t testModuleIndex)
{
    return FindFileFromPath(kSupportPath, gTestExpectationsFiles[testModuleIndex]);
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

    Optional<std::string> caseListPath = FindCaseListPath(mTestModuleIndex);
    if (!caseListPath.valid())
    {
        std::cerr << "Failed to find case list file." << std::endl;
        Die();
    }

    Optional<std::string> testExpectationsPath = FindTestExpectationsPath(mTestModuleIndex);
    if (!testExpectationsPath.valid())
    {
        std::cerr << "Failed to find test expectations file." << std::endl;
        Die();
    }

    GPUTestConfig::API api = GetDefaultAPIInfo()->second;
    // Set the API from the command line, or using the default platform API.
    if (gInitAPI)
    {
        api = gInitAPI->second;
    }

    GPUTestConfig testConfig = GPUTestConfig(api, gOptions.preRotation);

#if !defined(ANGLE_PLATFORM_ANDROID)
    // Note: These prints mess up parsing of test list when running on Android.
    std::cout << "Using test config with:" << std::endl;
    for (uint32_t condition : testConfig.getConditions())
    {
        const char *name = GetConditionName(condition);
        if (name != nullptr)
        {
            std::cout << "  " << name << std::endl;
        }
    }
#endif

    TestSuite *testSuite = TestSuite::GetInstance();

    if (!testSuite->loadTestExpectationsFromFileWithConfig(testConfig,
                                                           testExpectationsPath.value()))
    {
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

        std::string dEQPName = TrimString(inString, kWhitespaceASCII);
        if (dEQPName.empty())
            continue;
        std::string gTestName = DrawElementsToGoogleTestName(dEQPName);
        if (gTestName.empty())
            continue;

        int expectation = testSuite->getTestExpectation(dEQPName);
        mCaseInfoList.push_back(CaseInfo(dEQPName, gTestName, expectation));
    }

    if (testSuite->logAnyUnusedTestExpectations())
    {
        Die();
    }
}

bool IsPassingResult(dEQPTestResult result)
{
    // Check the global error flag for unexpected platform errors.
    if (gGlobalError)
    {
        gGlobalError = false;
        return false;
    }

    switch (result)
    {
        case dEQPTestResult::Fail:
        case dEQPTestResult::Exception:
            return false;

        default:
            return true;
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

        if (caseInfo.mExpectation == GPUTestExpectationsParser::kGpuTestSkip)
        {
            sSkippedTestCount++;
            std::cout << "Test skipped.\n";
            return;
        }

        TestSuite *testSuite = TestSuite::GetInstance();
        testSuite->maybeUpdateTestTimeout(caseInfo.mExpectation);

        gExpectError          = (caseInfo.mExpectation != GPUTestExpectationsParser::kGpuTestPass);
        dEQPTestResult result = deqp_libtester_run(caseInfo.mDEQPName.c_str());

        bool testSucceeded = IsPassingResult(result);

        if (!testSucceeded && caseInfo.mExpectation == GPUTestExpectationsParser::kGpuTestFlaky)
        {
            result        = deqp_libtester_run(caseInfo.mDEQPName.c_str());
            testSucceeded = IsPassingResult(result);
        }

        countTestResult(result);

        if (caseInfo.mExpectation == GPUTestExpectationsParser::kGpuTestPass ||
            caseInfo.mExpectation == GPUTestExpectationsParser::kGpuTestFlaky)
        {
            EXPECT_TRUE(testSucceeded);

            if (!testSucceeded)
            {
                sUnexpectedFailed.push_back(caseInfo.mDEQPName);
            }
        }
        else if (testSucceeded)
        {
            std::cout << "Test expected to fail but passed!" << std::endl;
            sUnexpectedPasses.push_back(caseInfo.mDEQPName);
        }
    }

    void countTestResult(dEQPTestResult result) const
    {
        switch (result)
        {
            case dEQPTestResult::Pass:
                sPassedTestCount++;
                break;
            case dEQPTestResult::Fail:
                sFailedTestCount++;
                break;
            case dEQPTestResult::NotSupported:
                sNotSupportedTestCount++;
                break;
            case dEQPTestResult::Exception:
                sTestExceptionCount++;
                break;
            default:
                std::cerr << "Unexpected test result code: " << static_cast<int>(result) << "\n";
                break;
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

        if (!sUnexpectedPasses.empty())
        {
            std::cout << GetTestStatLine("Unexpected Passed Count",
                                         std::to_string(sUnexpectedPasses.size()));
            for (const std::string &testName : sUnexpectedPasses)
            {
                std::cout << GetTestStatLine("Unexpected Passed Tests", testName);
            }
        }

        if (!sUnexpectedFailed.empty())
        {
            std::cout << GetTestStatLine("Unexpected Failed Count",
                                         std::to_string(sUnexpectedFailed.size()));
            for (const std::string &testName : sUnexpectedFailed)
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

    static std::vector<std::string> sUnexpectedFailed;
    static std::vector<std::string> sUnexpectedPasses;
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
template <size_t TestModuleIndex>
std::vector<std::string> dEQPTest<TestModuleIndex>::sUnexpectedFailed;
template <size_t TestModuleIndex>
std::vector<std::string> dEQPTest<TestModuleIndex>::sUnexpectedPasses;

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
    sUnexpectedPasses.clear();
    sUnexpectedFailed.clear();

    std::vector<const char *> argv;

    // Reserve one argument for the binary name.
    argv.push_back("");

    // Add init api.
    const char *targetApi    = gInitAPI ? gInitAPI->first : GetDefaultAPIName();
    std::string apiArgString = std::string(kdEQPEGLString) + targetApi;
    argv.push_back(apiArgString.c_str());

    // Add config name
    const char *targetConfigName = gEGLConfigName;
    std::string configArgString  = std::string(gdEQPEGLConfigNameString) + targetConfigName;
    argv.push_back(configArgString.c_str());

    // Hide SwiftShader window to prevent a race with Xvfb causing hangs on test bots
    if (gInitAPI && gInitAPI->second == GPUTestConfig::kAPISwiftShader)
    {
        argv.push_back("--deqp-visibility=hidden");
    }

    TestSuite *testSuite = TestSuite::GetInstance();

    std::stringstream logNameStream;
    logNameStream << "TestResults";
    if (testSuite->getBatchId() != -1)
    {
        logNameStream << "-Batch" << std::setfill('0') << std::setw(3) << testSuite->getBatchId();
    }
    logNameStream << ".qpa";

    std::stringstream logArgStream;
    logArgStream << "--deqp-log-filename="
                 << testSuite->reserveTestArtifactPath(logNameStream.str());

    std::string logNameString = logArgStream.str();
    argv.push_back(logNameString.c_str());

    if (!gLogImages)
    {
        argv.push_back("--deqp-log-images=disable");
    }

    // Flushing during multi-process execution punishes HDDs. http://anglebug.com/5157
    if (testSuite->getBatchId() != -1)
    {
        argv.push_back("--deqp-log-flush=disable");
    }

    // Add any additional flags specified from command line to be forwarded to dEQP.
    argv.insert(argv.end(), gdEQPForwardFlags.begin(), gdEQPForwardFlags.end());

    // Init the platform.
    if (!deqp_libtester_init_platform(static_cast<int>(argv.size()), argv.data(),
                                      reinterpret_cast<void *>(&HandlePlatformError), gOptions))
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

#ifdef ANGLE_DEQP_KHR_GLES2_TESTS
ANGLE_INSTANTIATE_DEQP_TEST_CASE(KHR_GLES2, 4);
#endif

#ifdef ANGLE_DEQP_KHR_GLES3_TESTS
ANGLE_INSTANTIATE_DEQP_TEST_CASE(KHR_GLES3, 5);
#endif

#ifdef ANGLE_DEQP_KHR_GLES31_TESTS
ANGLE_INSTANTIATE_DEQP_TEST_CASE(KHR_GLES31, 6);
#endif

#ifdef ANGLE_DEQP_KHR_GLES32_TESTS
ANGLE_INSTANTIATE_DEQP_TEST_CASE(KHR_GLES32, 7);
#endif

#ifdef ANGLE_DEQP_GLES3_ROTATE90_TESTS
ANGLE_INSTANTIATE_DEQP_TEST_CASE(GLES3_ROTATE90, 8);
#endif

#ifdef ANGLE_DEQP_GLES3_ROTATE180_TESTS
ANGLE_INSTANTIATE_DEQP_TEST_CASE(GLES3_ROTATE180, 9);
#endif

#ifdef ANGLE_DEQP_GLES3_ROTATE270_TESTS
ANGLE_INSTANTIATE_DEQP_TEST_CASE(GLES3_ROTATE270, 10);
#endif

#ifdef ANGLE_DEQP_GLES31_ROTATE90_TESTS
ANGLE_INSTANTIATE_DEQP_TEST_CASE(GLES31_ROTATE90, 11);
#endif

#ifdef ANGLE_DEQP_GLES31_ROTATE180_TESTS
ANGLE_INSTANTIATE_DEQP_TEST_CASE(GLES31_ROTATE180, 12);
#endif

#ifdef ANGLE_DEQP_GLES31_ROTATE270_TESTS
ANGLE_INSTANTIATE_DEQP_TEST_CASE(GLES31_ROTATE270, 13);
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

void HandlePreRotation(const char *preRotationString)
{
    std::istringstream argStream(preRotationString);

    uint32_t preRotation = 0;
    argStream >> preRotation;

    if (!argStream ||
        (preRotation != 0 && preRotation != 90 && preRotation != 180 && preRotation != 270))
    {
        std::cout << "Invalid PreRotation '" << preRotationString
                  << "'; must be either 0, 90, 180 or 270" << std::endl;
        exit(1);
    }

    gOptions.preRotation = preRotation;
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

void HandleLogImages(const char *logImagesString)
{
    if (strcmp(logImagesString, "enable") == 0)
    {
        gLogImages = true;
    }
    else if (strcmp(logImagesString, "disable") == 0)
    {
        gLogImages = false;
    }
    else
    {
        std::cout << "Error parsing log images setting. Use enable/disable.";
        exit(1);
    }
}
}  // anonymous namespace

// Called from main() to process command-line arguments.
void InitTestHarness(int *argc, char **argv)
{
    int argIndex = 0;
    while (argIndex < *argc)
    {
        if (strncmp(argv[argIndex], kdEQPEGLString, strlen(kdEQPEGLString)) == 0)
        {
            HandleDisplayType(argv[argIndex] + strlen(kdEQPEGLString));
        }
        else if (strncmp(argv[argIndex], kANGLEEGLString, strlen(kANGLEEGLString)) == 0)
        {
            HandleDisplayType(argv[argIndex] + strlen(kANGLEEGLString));
        }
        else if (strncmp(argv[argIndex], kANGLEPreRotation, strlen(kANGLEPreRotation)) == 0)
        {
            HandlePreRotation(argv[argIndex] + strlen(kANGLEPreRotation));
        }
        else if (strncmp(argv[argIndex], gdEQPEGLConfigNameString,
                         strlen(gdEQPEGLConfigNameString)) == 0)
        {
            HandleEGLConfigName(argv[argIndex] + strlen(gdEQPEGLConfigNameString));
        }
        else if (strncmp(argv[argIndex], kdEQPCaseString, strlen(kdEQPCaseString)) == 0)
        {
            HandleCaseName(argv[argIndex] + strlen(kdEQPCaseString), argc, argIndex, argv);
        }
        else if (strncmp(argv[argIndex], kVerboseString, strlen(kVerboseString)) == 0 ||
                 strcmp(argv[argIndex], "-v") == 0)
        {
            gVerbose = true;
        }
        else if (strncmp(argv[argIndex], gdEQPLogImagesString, strlen(gdEQPLogImagesString)) == 0)
        {
            HandleLogImages(argv[argIndex] + strlen(gdEQPLogImagesString));
        }
        else if (strncmp(argv[argIndex], kRenderDocString, strlen(kRenderDocString)) == 0)
        {
            gOptions.enableRenderDocCapture = true;
        }
        else if (strncmp(argv[argIndex], kNoRenderDocString, strlen(kNoRenderDocString)) == 0)
        {
            gOptions.enableRenderDocCapture = false;
        }
        else if (strncmp(argv[argIndex], kdEQPFlagsPrefix, strlen(kdEQPFlagsPrefix)) == 0)
        {
            gdEQPForwardFlags.push_back(argv[argIndex]);
        }
        argIndex++;
    }

    GPUTestConfig::API api = GetDefaultAPIInfo()->second;
    if (gInitAPI)
    {
        api = gInitAPI->second;
    }
    if (gOptions.preRotation != 0 && api != GPUTestConfig::kAPIVulkan &&
        api != GPUTestConfig::kAPISwiftShader)
    {
        std::cout << "PreRotation is only supported on Vulkan" << std::endl;
        exit(1);
    }
}
}  // namespace angle
