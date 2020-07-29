//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TestSuite:
//   Basic implementation of a test harness in ANGLE.

#include "TestSuite.h"

#include "common/debug.h"
#include "common/platform.h"
#include "common/string_utils.h"
#include "common/system_utils.h"
#include "util/Timer.h"

#include <time.h>
#include <fstream>
#include <unordered_map>

#include <gtest/gtest.h>
#include <rapidjson/document.h>
#include <rapidjson/filewritestream.h>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/prettywriter.h>

// We directly call into a function to register the parameterized tests. This saves spinning up
// a subprocess with a new gtest filter.
#include <gtest/../../src/gtest-internal-inl.h>

namespace js = rapidjson;

namespace angle
{
namespace
{
constexpr char kTestTimeoutArg[]       = "--test-timeout=";
constexpr char kFilterFileArg[]        = "--filter-file=";
constexpr char kResultFileArg[]        = "--results-file=";
constexpr char kHistogramJsonFileArg[] = "--histogram-json-file=";
#if defined(NDEBUG)
constexpr int kDefaultTestTimeout = 20;
#else
constexpr int kDefaultTestTimeout  = 60;
#endif
#if defined(NDEBUG)
constexpr int kDefaultBatchTimeout = 240;
#else
constexpr int kDefaultBatchTimeout = 600;
#endif
constexpr int kDefaultBatchSize = 1000;

const char *ParseFlagValue(const char *flag, const char *argument)
{
    if (strstr(argument, flag) == argument)
    {
        return argument + strlen(flag);
    }

    return nullptr;
}

bool ParseIntArg(const char *flag, const char *argument, int *valueOut)
{
    const char *value = ParseFlagValue(flag, argument);
    if (!value)
    {
        return false;
    }

    char *end            = nullptr;
    const long longValue = strtol(value, &end, 10);

    if (*end != '\0')
    {
        printf("Error parsing integer flag value.\n");
        exit(1);
    }

    if (longValue == LONG_MAX || longValue == LONG_MIN || static_cast<int>(longValue) != longValue)
    {
        printf("Overflow when parsing integer flag value.\n");
        exit(1);
    }

    *valueOut = static_cast<int>(longValue);
    return true;
}

bool ParseFlag(const char *expected, const char *actual, bool *flagOut)
{
    if (strcmp(expected, actual) == 0)
    {
        *flagOut = true;
        return true;
    }
    return false;
}

bool ParseStringArg(const char *flag, const char *argument, std::string *valueOut)
{
    const char *value = ParseFlagValue(flag, argument);
    if (!value)
    {
        return false;
    }

    *valueOut = value;
    return true;
}

void DeleteArg(int *argc, char **argv, int argIndex)
{
    // Shift the remainder of the argv list left by one.  Note that argv has (*argc + 1) elements,
    // the last one always being NULL.  The following loop moves the trailing NULL element as well.
    for (int index = argIndex; index < *argc; ++index)
    {
        argv[index] = argv[index + 1];
    }
    (*argc)--;
}

void AddArg(int *argc, char **argv, const char *arg)
{
    // This unsafe const_cast is necessary to work around gtest limitations.
    argv[*argc]     = const_cast<char *>(arg);
    argv[*argc + 1] = nullptr;
    (*argc)++;
}

const char *ResultTypeToString(TestResultType type)
{
    switch (type)
    {
        case TestResultType::Crash:
            return "CRASH";
        case TestResultType::Fail:
            return "FAIL";
        case TestResultType::Pass:
            return "PASS";
        case TestResultType::Skip:
            return "SKIP";
        case TestResultType::Timeout:
            return "TIMEOUT";
        case TestResultType::Unknown:
            return "UNKNOWN";
    }
}

TestResultType GetResultTypeFromString(const std::string &str)
{
    if (str == "CRASH")
        return TestResultType::Crash;
    if (str == "FAIL")
        return TestResultType::Fail;
    if (str == "PASS")
        return TestResultType::Pass;
    if (str == "SKIP")
        return TestResultType::Skip;
    if (str == "TIMEOUT")
        return TestResultType::Timeout;
    return TestResultType::Unknown;
}

js::Value ResultTypeToJSString(TestResultType type, js::Document::AllocatorType *allocator)
{
    js::Value jsName;
    jsName.SetString(ResultTypeToString(type), *allocator);
    return jsName;
}

bool WriteJsonFile(const std::string &outputFile, js::Document *doc)
{
    FILE *fp = fopen(outputFile.c_str(), "w");
    if (!fp)
    {
        return false;
    }

    constexpr size_t kBufferSize = 0xFFFF;
    std::vector<char> writeBuffer(kBufferSize);
    js::FileWriteStream os(fp, writeBuffer.data(), kBufferSize);
    js::PrettyWriter<js::FileWriteStream> writer(os);
    if (!doc->Accept(writer))
    {
        fclose(fp);
        return false;
    }
    fclose(fp);
    return true;
}

// Writes out a TestResults to the Chromium JSON Test Results format.
// https://chromium.googlesource.com/chromium/src.git/+/master/docs/testing/json_test_results_format.md
void WriteResultsFile(bool interrupted,
                      const TestResults &testResults,
                      const std::string &outputFile,
                      const char *testSuiteName)
{
    time_t ltime;
    time(&ltime);
    struct tm *timeinfo = gmtime(&ltime);
    ltime               = mktime(timeinfo);

    uint64_t secondsSinceEpoch = static_cast<uint64_t>(ltime);

    js::Document doc;
    doc.SetObject();

    js::Document::AllocatorType &allocator = doc.GetAllocator();

    doc.AddMember("interrupted", interrupted, allocator);
    doc.AddMember("path_delimiter", ".", allocator);
    doc.AddMember("version", 3, allocator);
    doc.AddMember("seconds_since_epoch", secondsSinceEpoch, allocator);

    js::Value testSuite;
    testSuite.SetObject();

    std::map<TestResultType, uint32_t> counts;

    for (const auto &resultIter : testResults.results)
    {
        const TestIdentifier &id = resultIter.first;
        const TestResult &result = resultIter.second;

        js::Value jsResult;
        jsResult.SetObject();

        counts[result.type]++;

        jsResult.AddMember("expected", "PASS", allocator);
        jsResult.AddMember("actual", ResultTypeToJSString(result.type, &allocator), allocator);

        js::Value times;
        times.SetArray();
        times.PushBack(result.elapsedTimeSeconds, allocator);

        jsResult.AddMember("times", times, allocator);

        char testName[500];
        id.sprintfName(testName);
        js::Value jsName;
        jsName.SetString(testName, allocator);

        testSuite.AddMember(jsName, jsResult, allocator);
    }

    js::Value numFailuresByType;
    numFailuresByType.SetObject();

    for (const auto &countIter : counts)
    {
        TestResultType type = countIter.first;
        uint32_t count      = countIter.second;

        js::Value jsCount(count);
        numFailuresByType.AddMember(ResultTypeToJSString(type, &allocator), jsCount, allocator);
    }

    doc.AddMember("num_failures_by_type", numFailuresByType, allocator);

    js::Value tests;
    tests.SetObject();
    tests.AddMember(js::StringRef(testSuiteName), testSuite, allocator);

    doc.AddMember("tests", tests, allocator);

    printf("Writing test results to %s\n", outputFile.c_str());

    if (!WriteJsonFile(outputFile, &doc))
    {
        printf("Error writing test results file.\n");
    }
}

void WriteHistogramJson(const TestResults &testResults,
                        const std::string &outputFile,
                        const char *testSuiteName)
{
    js::Document doc;
    doc.SetArray();

    // TODO: http://anglebug.com/4769 - Implement histogram output.

    printf("Writing histogram json to %s\n", outputFile.c_str());

    if (!WriteJsonFile(outputFile, &doc))
    {
        printf("Error writing histogram json file.\n");
    }
}

void WriteOutputFiles(bool interrupted,
                      const TestResults &testResults,
                      const std::string &resultsFile,
                      const std::string &histogramJsonOutputFile,
                      const char *testSuiteName)
{
    if (!resultsFile.empty())
    {
        WriteResultsFile(interrupted, testResults, resultsFile, testSuiteName);
    }

    if (!histogramJsonOutputFile.empty())
    {
        WriteHistogramJson(testResults, histogramJsonOutputFile, testSuiteName);
    }
}

void UpdateCurrentTestResult(const testing::TestResult &resultIn, TestResults *resultsOut)
{
    TestResult &resultOut = resultsOut->results[resultsOut->currentTest];

    // Note: Crashes and Timeouts are detected by the crash handler and a watchdog thread.
    if (resultIn.Skipped())
    {
        resultOut.type = TestResultType::Skip;
    }
    else if (resultIn.Failed())
    {
        resultOut.type = TestResultType::Fail;
    }
    else
    {
        resultOut.type = TestResultType::Pass;
    }

    resultOut.elapsedTimeSeconds = resultsOut->currentTestTimer.getElapsedTime();
}

TestIdentifier GetTestIdentifier(const testing::TestInfo &testInfo)
{
    return {testInfo.test_suite_name(), testInfo.name()};
}

class TestEventListener : public testing::EmptyTestEventListener
{
  public:
    // Note: TestResults is owned by the TestSuite. It should outlive TestEventListener.
    TestEventListener(const std::string &resultsFile,
                      const std::string &histogramJsonFile,
                      const char *testSuiteName,
                      TestResults *testResults)
        : mResultsFile(resultsFile),
          mHistogramJsonFile(histogramJsonFile),
          mTestSuiteName(testSuiteName),
          mTestResults(testResults)
    {}

    void OnTestStart(const testing::TestInfo &testInfo) override
    {
        std::lock_guard<std::mutex> guard(mTestResults->currentTestMutex);
        mTestResults->currentTest = GetTestIdentifier(testInfo);
        mTestResults->currentTestTimer.start();
    }

    void OnTestEnd(const testing::TestInfo &testInfo) override
    {
        std::lock_guard<std::mutex> guard(mTestResults->currentTestMutex);
        mTestResults->currentTestTimer.stop();
        const testing::TestResult &resultIn = *testInfo.result();
        UpdateCurrentTestResult(resultIn, mTestResults);
        mTestResults->currentTest = TestIdentifier();
    }

    void OnTestProgramEnd(const testing::UnitTest &testProgramInfo) override
    {
        std::lock_guard<std::mutex> guard(mTestResults->currentTestMutex);
        mTestResults->allDone = true;
        WriteOutputFiles(false, *mTestResults, mResultsFile, mHistogramJsonFile, mTestSuiteName);
    }

  private:
    std::string mResultsFile;
    std::string mHistogramJsonFile;
    const char *mTestSuiteName;
    TestResults *mTestResults;
};

bool IsTestDisabled(const testing::TestInfo &testInfo)
{
    return ::strstr(testInfo.name(), "DISABLED_") == testInfo.name();
}

using TestIdentifierFilter = std::function<bool(const TestIdentifier &id)>;

std::vector<TestIdentifier> FilterTests(std::map<TestIdentifier, FileLine> *fileLinesOut,
                                        TestIdentifierFilter filter,
                                        bool alsoRunDisabledTests)
{
    std::vector<TestIdentifier> tests;

    const testing::UnitTest &testProgramInfo = *testing::UnitTest::GetInstance();
    for (int suiteIndex = 0; suiteIndex < testProgramInfo.total_test_suite_count(); ++suiteIndex)
    {
        const testing::TestSuite &testSuite = *testProgramInfo.GetTestSuite(suiteIndex);
        for (int testIndex = 0; testIndex < testSuite.total_test_count(); ++testIndex)
        {
            const testing::TestInfo &testInfo = *testSuite.GetTestInfo(testIndex);
            TestIdentifier id                 = GetTestIdentifier(testInfo);
            if (filter(id) && (!IsTestDisabled(testInfo) || alsoRunDisabledTests))
            {
                tests.emplace_back(id);

                if (fileLinesOut)
                {
                    (*fileLinesOut)[id] = {testInfo.file(), testInfo.line()};
                }
            }
        }
    }

    return tests;
}

std::vector<TestIdentifier> GetFilteredTests(std::map<TestIdentifier, FileLine> *fileLinesOut,
                                             bool alsoRunDisabledTests)
{
    TestIdentifierFilter gtestIDFilter = [](const TestIdentifier &id) {
        return testing::internal::UnitTestOptions::FilterMatchesTest(id.testSuiteName, id.testName);
    };

    return FilterTests(fileLinesOut, gtestIDFilter, alsoRunDisabledTests);
}

std::vector<TestIdentifier> GetShardTests(const std::vector<TestIdentifier> &allTests,
                                          int shardIndex,
                                          int shardCount,
                                          std::map<TestIdentifier, FileLine> *fileLinesOut,
                                          bool alsoRunDisabledTests)
{
    std::vector<TestIdentifier> shardTests;

    for (int testIndex = shardIndex; testIndex < static_cast<int>(allTests.size());
         testIndex += shardCount)
    {
        shardTests.emplace_back(allTests[testIndex]);
    }

    return shardTests;
}

std::string GetTestFilter(const std::vector<TestIdentifier> &tests)
{
    std::stringstream filterStream;

    filterStream << "--gtest_filter=";

    for (size_t testIndex = 0; testIndex < tests.size(); ++testIndex)
    {
        if (testIndex != 0)
        {
            filterStream << ":";
        }

        filterStream << tests[testIndex];
    }

    return filterStream.str();
}

std::string ParseTestSuiteName(const char *executable)
{
    const char *baseNameStart = strrchr(executable, GetPathSeparator());
    if (!baseNameStart)
    {
        baseNameStart = executable;
    }
    else
    {
        baseNameStart++;
    }

    const char *suffix = GetExecutableExtension();
    size_t suffixLen   = strlen(suffix);
    if (suffixLen == 0)
    {
        return baseNameStart;
    }

    if (!EndsWith(baseNameStart, suffix))
    {
        return baseNameStart;
    }

    return std::string(baseNameStart, baseNameStart + strlen(baseNameStart) - suffixLen);
}

bool GetTestResultsFromJSON(const js::Document &document, TestResults *resultsOut)
{
    if (!document.HasMember("tests") || !document["tests"].IsObject())
    {
        return false;
    }

    const js::Value::ConstObject &tests = document["tests"].GetObject();
    if (tests.MemberCount() != 1)
    {
        return false;
    }

    const js::Value::Member &suite = *tests.MemberBegin();
    if (!suite.value.IsObject())
    {
        return false;
    }

    const js::Value::ConstObject &actual = suite.value.GetObject();

    for (auto iter = actual.MemberBegin(); iter != actual.MemberEnd(); ++iter)
    {
        // Get test identifier.
        const js::Value &name = iter->name;
        if (!name.IsString())
        {
            return false;
        }

        TestIdentifier id;
        if (!TestIdentifier::ParseFromString(name.GetString(), &id))
        {
            return false;
        }

        // Get test result.
        const js::Value &value = iter->value;
        if (!value.IsObject())
        {
            return false;
        }

        const js::Value::ConstObject &obj = value.GetObject();
        if (!obj.HasMember("expected") || !obj.HasMember("actual"))
        {
            return false;
        }

        const js::Value &expected = obj["expected"];
        const js::Value &actual   = obj["actual"];

        if (!expected.IsString() || !actual.IsString())
        {
            return false;
        }

        const std::string expectedStr = expected.GetString();
        const std::string actualStr   = actual.GetString();

        if (expectedStr != "PASS")
        {
            return false;
        }

        TestResultType resultType = GetResultTypeFromString(actualStr);
        if (resultType == TestResultType::Unknown)
        {
            return false;
        }

        double elapsedTimeSeconds = 0.0;
        if (obj.HasMember("times"))
        {
            const js::Value &times = obj["times"];
            if (!times.IsArray())
            {
                return false;
            }

            const js::Value::ConstArray &timesArray = times.GetArray();
            if (timesArray.Size() != 1 || !timesArray[0].IsDouble())
            {
                return false;
            }

            elapsedTimeSeconds = timesArray[0].GetDouble();
        }

        TestResult &result        = resultsOut->results[id];
        result.elapsedTimeSeconds = elapsedTimeSeconds;
        result.type               = resultType;
    }

    return true;
}

bool MergeTestResults(const TestResults &input, TestResults *output)
{
    for (const auto &resultsIter : input.results)
    {
        const TestIdentifier &id      = resultsIter.first;
        const TestResult &inputResult = resultsIter.second;
        TestResult &outputResult      = output->results[id];

        // This should probably handle situations where a test is run more than once.
        if (inputResult.type != TestResultType::Skip)
        {
            if (outputResult.type != TestResultType::Skip)
            {
                printf("Warning: duplicate entry for %s.%s.\n", id.testSuiteName.c_str(),
                       id.testName.c_str());
                return false;
            }

            outputResult.elapsedTimeSeconds = inputResult.elapsedTimeSeconds;
            outputResult.type               = inputResult.type;
        }
    }

    return true;
}

void PrintTestOutputSnippet(const TestIdentifier &id,
                            const TestResult &result,
                            const std::string &fullOutput)
{
    std::stringstream nameStream;
    nameStream << id;
    std::string fullName = nameStream.str();

    size_t runPos = fullOutput.find(std::string("[ RUN      ] ") + fullName);
    if (runPos == std::string::npos)
    {
        printf("Cannot locate test output snippet.\n");
        return;
    }

    size_t endPos = fullOutput.find(std::string("[  FAILED  ] ") + fullName, runPos);
    // Only clip the snippet to the "OK" message if the test really
    // succeeded. It still might have e.g. crashed after printing it.
    if (endPos == std::string::npos && result.type == TestResultType::Pass)
    {
        endPos = fullOutput.find(std::string("[       OK ] ") + fullName, runPos);
    }
    if (endPos != std::string::npos)
    {
        size_t newline_pos = fullOutput.find("\n", endPos);
        if (newline_pos != std::string::npos)
            endPos = newline_pos + 1;
    }

    std::cout << "\n";
    if (endPos != std::string::npos)
    {
        std::cout << fullOutput.substr(runPos, endPos - runPos);
    }
    else
    {
        std::cout << fullOutput.substr(runPos);
    }
    std::cout << "\n";
}

std::string GetConfigNameFromTestIdentifier(const TestIdentifier &id)
{
    size_t slashPos = id.testName.find('/');
    if (slashPos == std::string::npos)
    {
        return "default";
    }

    size_t doubleUnderscorePos = id.testName.find("__");
    if (doubleUnderscorePos == std::string::npos)
    {
        return id.testName.substr(slashPos + 1);
    }
    else
    {
        return id.testName.substr(slashPos + 1, doubleUnderscorePos - slashPos - 1);
    }
}

TestQueue BatchTests(const std::vector<TestIdentifier> &tests, int batchSize)
{
    // First sort tests by configuration.
    std::unordered_map<std::string, std::vector<TestIdentifier>> testsSortedByConfig;
    for (const TestIdentifier &id : tests)
    {
        std::string config = GetConfigNameFromTestIdentifier(id);
        testsSortedByConfig[config].push_back(id);
    }

    // Then group into batches by 'batchSize'.
    TestQueue testQueue;
    for (const auto &configAndIds : testsSortedByConfig)
    {
        const std::vector<TestIdentifier> &configTests = configAndIds.second;
        std::vector<TestIdentifier> batchTests;
        for (const TestIdentifier &id : configTests)
        {
            if (batchTests.size() >= static_cast<size_t>(batchSize))
            {
                testQueue.emplace(std::move(batchTests));
                ASSERT(batchTests.empty());
            }
            batchTests.push_back(id);
        }

        if (!batchTests.empty())
        {
            testQueue.emplace(std::move(batchTests));
        }
    }

    return testQueue;
}
}  // namespace

TestIdentifier::TestIdentifier() = default;

TestIdentifier::TestIdentifier(const std::string &suiteNameIn, const std::string &nameIn)
    : testSuiteName(suiteNameIn), testName(nameIn)
{}

TestIdentifier::TestIdentifier(const TestIdentifier &other) = default;

TestIdentifier::~TestIdentifier() = default;

TestIdentifier &TestIdentifier::operator=(const TestIdentifier &other) = default;

void TestIdentifier::sprintfName(char *outBuffer) const
{
    sprintf(outBuffer, "%s.%s", testSuiteName.c_str(), testName.c_str());
}

// static
bool TestIdentifier::ParseFromString(const std::string &str, TestIdentifier *idOut)
{
    size_t separator = str.find(".");
    if (separator == std::string::npos)
    {
        return false;
    }

    idOut->testSuiteName = str.substr(0, separator);
    idOut->testName      = str.substr(separator + 1, str.length() - separator - 1);
    return true;
}

TestResults::TestResults() = default;

TestResults::~TestResults() = default;

ProcessInfo::ProcessInfo() = default;

ProcessInfo &ProcessInfo::operator=(ProcessInfo &&rhs)
{
    process         = std::move(rhs.process);
    testsInBatch    = std::move(rhs.testsInBatch);
    resultsFileName = std::move(rhs.resultsFileName);
    filterFileName  = std::move(rhs.filterFileName);
    commandLine     = std::move(rhs.commandLine);
    return *this;
}

ProcessInfo::~ProcessInfo() = default;

ProcessInfo::ProcessInfo(ProcessInfo &&other)
{
    *this = std::move(other);
}

TestSuite::TestSuite(int *argc, char **argv)
    : mShardCount(-1),
      mShardIndex(-1),
      mBotMode(false),
      mDebugTestGroups(false),
      mBatchSize(kDefaultBatchSize),
      mCurrentResultCount(0),
      mTotalResultCount(0),
      mMaxProcesses(NumberOfProcessors()),
      mTestTimeout(kDefaultTestTimeout),
      mBatchTimeout(kDefaultBatchTimeout)
{
    Optional<int> filterArgIndex;
    bool alsoRunDisabledTests = false;

#if defined(ANGLE_PLATFORM_WINDOWS)
    testing::GTEST_FLAG(catch_exceptions) = false;
#endif

    // Note that the crash callback must be owned and not use global constructors.
    mCrashCallback = [this]() { onCrashOrTimeout(TestResultType::Crash); };
    InitCrashHandler(&mCrashCallback);

    if (*argc <= 0)
    {
        printf("Missing test arguments.\n");
        exit(1);
    }

    mTestExecutableName = argv[0];
    mTestSuiteName      = ParseTestSuiteName(mTestExecutableName.c_str());

    for (int argIndex = 1; argIndex < *argc;)
    {
        if (parseSingleArg(argv[argIndex]))
        {
            DeleteArg(argc, argv, argIndex);
            continue;
        }

        if (ParseFlagValue("--gtest_filter=", argv[argIndex]))
        {
            filterArgIndex = argIndex;
        }
        else
        {
            // Don't include disabled tests in test lists unless the user asks for them.
            if (strcmp("--gtest_also_run_disabled_tests", argv[argIndex]) == 0)
            {
                alsoRunDisabledTests = true;
            }

            mGoogleTestCommandLineArgs.push_back(argv[argIndex]);
        }
        ++argIndex;
    }

    if ((mShardIndex >= 0) != (mShardCount > 1))
    {
        printf("Shard index and shard count must be specified together.\n");
        exit(1);
    }

    if (!mFilterFile.empty())
    {
        if (filterArgIndex.valid())
        {
            printf("Cannot use gtest_filter in conjunction with a filter file.\n");
            exit(1);
        }

        uint32_t fileSize = 0;
        if (!GetFileSize(mFilterFile.c_str(), &fileSize))
        {
            printf("Error getting filter file size: %s\n", mFilterFile.c_str());
            exit(1);
        }

        std::vector<char> fileContents(fileSize + 1, 0);
        if (!ReadEntireFileToString(mFilterFile.c_str(), fileContents.data(), fileSize))
        {
            printf("Error loading filter file: %s\n", mFilterFile.c_str());
            exit(1);
        }
        mFilterString.assign(fileContents.data());

        if (mFilterString.substr(0, strlen("--gtest_filter=")) != std::string("--gtest_filter="))
        {
            printf("Filter file must start with \"--gtest_filter=\".");
            exit(1);
        }

        // Note that we only add a filter string if we previously deleted a shader filter file
        // argument. So we will have space for the new filter string in argv.
        AddArg(argc, argv, mFilterString.c_str());
    }

    // Call into gtest internals to force parameterized test name registration.
    testing::internal::UnitTestImpl *impl = testing::internal::GetUnitTestImpl();
    impl->RegisterParameterizedTests();

    // Initialize internal GoogleTest filter arguments so we can call "FilterMatchesTest".
    testing::internal::ParseGoogleTestFlagsOnly(argc, argv);

    std::vector<TestIdentifier> testSet = GetFilteredTests(&mTestFileLines, alsoRunDisabledTests);

    if (mShardCount > 0)
    {
        testSet =
            GetShardTests(testSet, mShardIndex, mShardCount, &mTestFileLines, alsoRunDisabledTests);

        if (!mBotMode)
        {
            mFilterString = GetTestFilter(testSet);

            if (filterArgIndex.valid())
            {
                argv[filterArgIndex.value()] = const_cast<char *>(mFilterString.c_str());
            }
            else
            {
                // Note that we only add a filter string if we previously deleted a shard
                // index/count argument. So we will have space for the new filter string in argv.
                AddArg(argc, argv, mFilterString.c_str());
            }

            // Force-re-initialize GoogleTest flags to load the shard filter.
            testing::internal::ParseGoogleTestFlagsOnly(argc, argv);
        }
    }

    if (mBotMode)
    {
        // Split up test batches.
        mTestQueue = BatchTests(testSet, mBatchSize);

        if (mDebugTestGroups)
        {
            std::cout << "Test Groups:\n";

            while (!mTestQueue.empty())
            {
                const std::vector<TestIdentifier> &tests = mTestQueue.front();
                std::cout << tests[0] << " (" << static_cast<int>(tests.size()) << ")\n";
                mTestQueue.pop();
            }

            exit(0);
        }
    }

    testing::InitGoogleTest(argc, argv);

    mTotalResultCount = testSet.size();

    if ((mBotMode || !mResultsDirectory.empty()) && mResultsFile.empty())
    {
        // Create a default output file in bot mode.
        mResultsFile = "output.json";
    }

    if (!mResultsDirectory.empty())
    {
        std::stringstream resultFileName;
        resultFileName << mResultsDirectory << GetPathSeparator() << mResultsFile;
        mResultsFile = resultFileName.str();
    }

    if (!mResultsFile.empty() || !mHistogramJsonFile.empty())
    {
        testing::TestEventListeners &listeners = testing::UnitTest::GetInstance()->listeners();
        listeners.Append(new TestEventListener(mResultsFile, mHistogramJsonFile,
                                               mTestSuiteName.c_str(), &mTestResults));

        std::vector<TestIdentifier> testList = GetFilteredTests(nullptr, alsoRunDisabledTests);

        for (const TestIdentifier &id : testList)
        {
            mTestResults.results[id].type = TestResultType::Skip;
        }
    }
}

TestSuite::~TestSuite()
{
    if (mWatchdogThread.joinable())
    {
        mWatchdogThread.detach();
    }
    TerminateCrashHandler();
}

bool TestSuite::parseSingleArg(const char *argument)
{
    // Note: Flags should be documented in README.md.
    return (ParseIntArg("--shard-count=", argument, &mShardCount) ||
            ParseIntArg("--shard-index=", argument, &mShardIndex) ||
            ParseIntArg("--batch-size=", argument, &mBatchSize) ||
            ParseIntArg("--max-processes=", argument, &mMaxProcesses) ||
            ParseIntArg(kTestTimeoutArg, argument, &mTestTimeout) ||
            ParseIntArg("--batch-timeout=", argument, &mBatchTimeout) ||
            ParseStringArg("--results-directory=", argument, &mResultsDirectory) ||
            ParseStringArg(kResultFileArg, argument, &mResultsFile) ||
            ParseStringArg("--isolated-script-test-output", argument, &mResultsFile) ||
            ParseStringArg(kFilterFileArg, argument, &mFilterFile) ||
            ParseStringArg(kHistogramJsonFileArg, argument, &mHistogramJsonFile) ||
            ParseStringArg("--isolated-script-perf-test-output", argument, &mHistogramJsonFile) ||
            ParseFlag("--bot-mode", argument, &mBotMode) ||
            ParseFlag("--debug-test-groups", argument, &mDebugTestGroups));
}

void TestSuite::onCrashOrTimeout(TestResultType crashOrTimeout)
{
    if (mTestResults.currentTest.valid())
    {
        TestResult &result        = mTestResults.results[mTestResults.currentTest];
        result.type               = crashOrTimeout;
        result.elapsedTimeSeconds = mTestResults.currentTestTimer.getElapsedTime();
    }

    if (mResultsFile.empty())
    {
        printf("No results file specified.\n");
        return;
    }

    WriteOutputFiles(true, mTestResults, mResultsFile, mHistogramJsonFile, mTestSuiteName.c_str());
}

bool TestSuite::launchChildTestProcess(const std::vector<TestIdentifier> &testsInBatch)
{
    constexpr uint32_t kMaxPath = 1000;

    // Create a temporary file to store the test list
    ProcessInfo processInfo;

    char filterBuffer[kMaxPath] = {};
    if (!CreateTemporaryFile(filterBuffer, kMaxPath))
    {
        std::cerr << "Error creating temporary file for test list.\n";
        return false;
    }
    processInfo.filterFileName.assign(filterBuffer);

    std::string filterString = GetTestFilter(testsInBatch);

    FILE *fp = fopen(processInfo.filterFileName.c_str(), "w");
    if (!fp)
    {
        std::cerr << "Error opening temporary file for test list.\n";
        return false;
    }
    fprintf(fp, "%s", filterString.c_str());
    fclose(fp);

    std::string filterFileArg = kFilterFileArg + processInfo.filterFileName;

    // Create a temporary file to store the test output.
    char resultsBuffer[kMaxPath] = {};
    if (!CreateTemporaryFile(resultsBuffer, kMaxPath))
    {
        std::cerr << "Error creating temporary file for test list.\n";
        return false;
    }
    processInfo.resultsFileName.assign(resultsBuffer);

    std::string resultsFileArg = kResultFileArg + processInfo.resultsFileName;

    // Construct commandline for child process.
    std::vector<const char *> args;

    args.push_back(mTestExecutableName.c_str());
    args.push_back(filterFileArg.c_str());
    args.push_back(resultsFileArg.c_str());

    for (const std::string &arg : mGoogleTestCommandLineArgs)
    {
        args.push_back(arg.c_str());
    }

    std::string timeoutStr;
    if (mTestTimeout != kDefaultTestTimeout)
    {
        std::stringstream timeoutStream;
        timeoutStream << kTestTimeoutArg << mTestTimeout;
        timeoutStr = timeoutStream.str();
        args.push_back(timeoutStr.c_str());
    }

    // Launch child process and wait for completion.
    processInfo.process = LaunchProcess(args, true, true);

    if (!processInfo.process->started())
    {
        std::cerr << "Error launching child process.\n";
        return false;
    }

    std::stringstream commandLineStr;
    for (const char *arg : args)
    {
        commandLineStr << arg << " ";
    }

    processInfo.commandLine  = commandLineStr.str();
    processInfo.testsInBatch = testsInBatch;
    mCurrentProcesses.emplace_back(std::move(processInfo));
    return true;
}

bool TestSuite::finishProcess(ProcessInfo *processInfo)
{
    // Get test results and merge into master list.
    TestResults batchResults;

    if (!GetTestResultsFromFile(processInfo->resultsFileName.c_str(), &batchResults))
    {
        std::cerr << "Error reading test results from child process.\n";
        return false;
    }

    if (!MergeTestResults(batchResults, &mTestResults))
    {
        std::cerr << "Error merging batch test results.\n";
        return false;
    }

    // Process results and print unexpected errors.
    for (const auto &resultIter : batchResults.results)
    {
        const TestIdentifier &id = resultIter.first;
        const TestResult &result = resultIter.second;

        // Skip results aren't procesed since they're added back to the test queue below.
        if (result.type == TestResultType::Skip)
        {
            continue;
        }

        mCurrentResultCount++;
        printf("[%d/%d] %s.%s", mCurrentResultCount, mTotalResultCount, id.testSuiteName.c_str(),
               id.testName.c_str());

        if (result.type == TestResultType::Pass)
        {
            printf(" (%g ms)\n", result.elapsedTimeSeconds * 1000.0);
        }
        else
        {
            printf(" (%s)\n", ResultTypeToString(result.type));

            const std::string &batchStdout = processInfo->process->getStdout();
            PrintTestOutputSnippet(id, result, batchStdout);
        }
    }

    // On unexpected exit, re-queue any unfinished tests.
    if (processInfo->process->getExitCode() != 0)
    {
        std::vector<TestIdentifier> unfinishedTests;

        for (const auto &resultIter : batchResults.results)
        {
            const TestIdentifier &id = resultIter.first;
            const TestResult &result = resultIter.second;

            if (result.type == TestResultType::Skip)
            {
                unfinishedTests.push_back(id);
            }
        }

        mTestQueue.emplace(std::move(unfinishedTests));
    }

    // Clean up any dirty temporary files.
    for (const std::string &tempFile : {processInfo->filterFileName, processInfo->resultsFileName})
    {
        // Note: we should be aware that this cleanup won't happen if the harness itself crashes.
        // If this situation comes up in the future we should add crash cleanup to the harness.
        if (!angle::DeleteFile(tempFile.c_str()))
        {
            std::cerr << "Warning: Error cleaning up temp file: " << tempFile << "\n";
        }
    }

    processInfo->process.reset();
    return true;
}

int TestSuite::run()
{
    // Run tests serially.
    if (!mBotMode)
    {
        startWatchdog();
        return RUN_ALL_TESTS();
    }

    constexpr double kIdleMessageTimeout = 5.0;

    Timer messageTimer;
    messageTimer.start();

    while (!mTestQueue.empty() || !mCurrentProcesses.empty())
    {
        bool progress = false;

        // Spawn a process if needed and possible.
        while (static_cast<int>(mCurrentProcesses.size()) < mMaxProcesses && !mTestQueue.empty())
        {
            std::vector<TestIdentifier> testsInBatch = mTestQueue.front();
            mTestQueue.pop();

            if (!launchChildTestProcess(testsInBatch))
            {
                return 1;
            }

            progress = true;
        }

        // Check for process completion.
        for (auto processIter = mCurrentProcesses.begin(); processIter != mCurrentProcesses.end();)
        {
            ProcessInfo &processInfo = *processIter;
            if (processInfo.process->finished())
            {
                if (!finishProcess(&processInfo))
                {
                    return 1;
                }
                processIter = mCurrentProcesses.erase(processIter);
                progress    = true;
            }
            else if (processInfo.process->getElapsedTimeSeconds() > mBatchTimeout)
            {
                // Terminate the process and record timeouts for the batch.
                // Because we can't determine which sub-test caused a timeout, record the whole
                // batch as a timeout failure. Can be improved by using socket message passing.
                if (!processInfo.process->kill())
                {
                    return 1;
                }
                for (const TestIdentifier &testIdentifier : processInfo.testsInBatch)
                {
                    // Because the whole batch failed we can't know how long each test took.
                    mTestResults.results[testIdentifier].type = TestResultType::Timeout;
                }

                processIter = mCurrentProcesses.erase(processIter);
                progress    = true;
            }
            else
            {
                processIter++;
            }
        }

        if (!progress && messageTimer.getElapsedTime() > kIdleMessageTimeout)
        {
            for (const ProcessInfo &processInfo : mCurrentProcesses)
            {
                double processTime = processInfo.process->getElapsedTimeSeconds();
                if (processTime > kIdleMessageTimeout)
                {
                    printf("Running for %d seconds: %s\n", static_cast<int>(processTime),
                           processInfo.commandLine.c_str());
                }
            }

            messageTimer.start();
        }

        // Sleep briefly and continue.
        angle::Sleep(10);
    }

    // Dump combined results.
    WriteOutputFiles(true, mTestResults, mResultsFile, mHistogramJsonFile, mTestSuiteName.c_str());

    return printFailuresAndReturnCount() == 0;
}

int TestSuite::printFailuresAndReturnCount() const
{
    std::vector<std::string> failures;

    for (const auto &resultIter : mTestResults.results)
    {
        const TestIdentifier &id = resultIter.first;
        const TestResult &result = resultIter.second;

        if (result.type != TestResultType::Pass)
        {
            const FileLine &fileLine = mTestFileLines.find(id)->second;

            std::stringstream failureMessage;
            failureMessage << id << " (" << fileLine.file << ":" << fileLine.line << ") ("
                           << ResultTypeToString(result.type) << ")";
            failures.emplace_back(failureMessage.str());
        }
    }

    if (failures.empty())
        return 0;

    printf("%zu test%s failed:\n", failures.size(), failures.size() > 1 ? "s" : "");
    for (const std::string &failure : failures)
    {
        printf("    %s\n", failure.c_str());
    }

    return static_cast<int>(failures.size());
}

void TestSuite::startWatchdog()
{
    auto watchdogMain = [this]() {
        do
        {
            {
                std::lock_guard<std::mutex> guard(mTestResults.currentTestMutex);
                if (mTestResults.currentTestTimer.getElapsedTime() >
                    static_cast<double>(mTestTimeout))
                {
                    onCrashOrTimeout(TestResultType::Timeout);
                    exit(2);
                }

                if (mTestResults.allDone)
                    return;
            }

            angle::Sleep(1000);
        } while (true);
    };
    mWatchdogThread = std::thread(watchdogMain);
}

bool GetTestResultsFromFile(const char *fileName, TestResults *resultsOut)
{
    std::ifstream ifs(fileName);
    if (!ifs.is_open())
    {
        std::cerr << "Error opening " << fileName << "\n";
        return false;
    }

    js::IStreamWrapper ifsWrapper(ifs);
    js::Document document;
    document.ParseStream(ifsWrapper);

    if (document.HasParseError())
    {
        std::cerr << "Parse error reading JSON document: " << document.GetParseError() << "\n";
        return false;
    }

    if (!GetTestResultsFromJSON(document, resultsOut))
    {
        std::cerr << "Error getting test results from JSON.\n";
        return false;
    }

    return true;
}

const char *TestResultTypeToString(TestResultType type)
{
    switch (type)
    {
        case TestResultType::Crash:
            return "Crash";
        case TestResultType::Fail:
            return "Fail";
        case TestResultType::Skip:
            return "Skip";
        case TestResultType::Pass:
            return "Pass";
        case TestResultType::Timeout:
            return "Timeout";
        case TestResultType::Unknown:
            return "Unknown";
    }
}
}  // namespace angle
