//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TestSuite:
//   Basic implementation of a test harness in ANGLE.

#ifndef ANGLE_TESTS_TEST_UTILS_TEST_SUITE_H_
#define ANGLE_TESTS_TEST_UTILS_TEST_SUITE_H_

#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

#include "HistogramWriter.h"
#include "tests/test_expectations/GPUTestExpectationsParser.h"
#include "util/test_utils.h"

namespace angle
{
struct TestIdentifier
{
    TestIdentifier();
    TestIdentifier(const std::string &suiteNameIn, const std::string &nameIn);
    TestIdentifier(const TestIdentifier &other);
    ~TestIdentifier();

    TestIdentifier &operator=(const TestIdentifier &other);

    static bool ParseFromString(const std::string &str, TestIdentifier *idOut);

    bool valid() const { return !testName.empty(); }
    void sprintfName(char *outBuffer) const;

    std::string testSuiteName;
    std::string testName;
};

inline bool operator<(const TestIdentifier &a, const TestIdentifier &b)
{
    return std::tie(a.testSuiteName, a.testName) < std::tie(b.testSuiteName, b.testName);
}

inline bool operator==(const TestIdentifier &a, const TestIdentifier &b)
{
    return std::tie(a.testSuiteName, a.testName) == std::tie(b.testSuiteName, b.testName);
}

inline std::ostream &operator<<(std::ostream &os, const TestIdentifier &id)
{
    return os << id.testSuiteName << "." << id.testName;
}

enum class TestResultType
{
    Crash,
    Fail,
    NoResult,
    Pass,
    Skip,
    Timeout,
    Unknown,
};

const char *TestResultTypeToString(TestResultType type);

struct TestResult
{
    TestResultType type                    = TestResultType::NoResult;
    std::vector<double> elapsedTimeSeconds = std::vector<double>({0.0});
    uint32_t flakyFailures                 = 0;
};

inline bool operator==(const TestResult &a, const TestResult &b)
{
    return a.type == b.type;
}

inline std::ostream &operator<<(std::ostream &os, const TestResult &result)
{
    return os << TestResultTypeToString(result.type);
}

struct TestResults
{
    TestResults();
    ~TestResults();

    std::map<TestIdentifier, TestResult> results;
    std::mutex currentTestMutex;
    TestIdentifier currentTest;
    Timer currentTestTimer;
    double currentTestTimeout = 0.0;
    bool allDone              = false;
    std::string testArtifactsFakeTestName;
    std::vector<std::string> testArtifactPaths;
};

struct FileLine
{
    const char *file;
    int line;
};

struct ProcessInfo : angle::NonCopyable
{
    ProcessInfo();
    ~ProcessInfo();
    ProcessInfo(ProcessInfo &&other);
    ProcessInfo &operator=(ProcessInfo &&rhs);

    ProcessHandle process;
    std::vector<TestIdentifier> testsInBatch;
    std::string resultsFileName;
    std::string filterFileName;
    std::string commandLine;
    std::string filterString;
};

using TestQueue = std::queue<std::vector<TestIdentifier>>;

class TestSuite
{
  public:
    TestSuite(int *argc, char **argv);
    ~TestSuite();

    int run();
    void onCrashOrTimeout(TestResultType crashOrTimeout);
    void addHistogramSample(const std::string &measurement,
                            const std::string &story,
                            double value,
                            const std::string &units);

    static TestSuite *GetInstance() { return mInstance; }

    // Returns the path to the artifact in the output directory.
    bool hasTestArtifactsDirectory() const;
    std::string reserveTestArtifactPath(const std::string &artifactName);

    int getShardIndex() const { return mShardIndex; }
    int getBatchId() const { return mBatchId; }

    // Test expectation processing.
    bool loadTestExpectationsFromFileWithConfig(const GPUTestConfig &config,
                                                const std::string &fileName);
    bool loadAllTestExpectationsFromFile(const std::string &fileName);
    int32_t getTestExpectation(const std::string &testName);
    void maybeUpdateTestTimeout(uint32_t testExpectation);
    int32_t getTestExpectationWithConfigAndUpdateTimeout(const GPUTestConfig &config,
                                                         const std::string &testName);
    bool logAnyUnusedTestExpectations();
    void setTestExpectationsAllowMask(uint32_t mask)
    {
        mTestExpectationsParser.setTestExpectationsAllowMask(mask);
    }

  private:
    bool parseSingleArg(const char *argument);
    bool launchChildTestProcess(uint32_t batchId, const std::vector<TestIdentifier> &testsInBatch);
    bool finishProcess(ProcessInfo *processInfo);
    int printFailuresAndReturnCount() const;
    void startWatchdog();
    void dumpTestExpectationsErrorMessages();
    int getSlowTestTimeout() const;

    static TestSuite *mInstance;

    std::string mTestExecutableName;
    std::string mTestSuiteName;
    TestQueue mTestQueue;
    std::string mFilterString;
    std::string mFilterFile;
    std::string mResultsDirectory;
    std::string mResultsFile;
    std::string mHistogramJsonFile;
    int mShardCount;
    int mShardIndex;
    angle::CrashCallback mCrashCallback;
    TestResults mTestResults;
    bool mBotMode;
    bool mDebugTestGroups;
    bool mGTestListTests;
    bool mListTests;
    bool mPrintTestStdout;
    bool mDisableCrashHandler;
    int mBatchSize;
    int mCurrentResultCount;
    int mTotalResultCount;
    int mMaxProcesses;
    int mTestTimeout;
    int mBatchTimeout;
    int mBatchId;
    int mFlakyRetries;
    int mMaxFailures;
    int mFailureCount;
    bool mModifiedPreferredDevice;
    std::vector<std::string> mChildProcessArgs;
    std::map<TestIdentifier, FileLine> mTestFileLines;
    std::vector<ProcessInfo> mCurrentProcesses;
    std::thread mWatchdogThread;
    HistogramWriter mHistogramWriter;
    std::string mTestArtifactDirectory;
    GPUTestExpectationsParser mTestExpectationsParser;
};

bool GetTestResultsFromFile(const char *fileName, TestResults *resultsOut);
}  // namespace angle

#endif  // ANGLE_TESTS_TEST_UTILS_TEST_SUITE_H_
