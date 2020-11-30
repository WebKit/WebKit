//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TestSuite:
//   Basic implementation of a test harness in ANGLE.

#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

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
    Skip,
    Pass,
    Timeout,
    Unknown,
};

const char *TestResultTypeToString(TestResultType type);

struct TestResult
{
    TestResultType type       = TestResultType::Skip;
    double elapsedTimeSeconds = 0.0;
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
    bool allDone = false;
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
};

using TestQueue = std::queue<std::vector<TestIdentifier>>;

class TestSuite
{
  public:
    TestSuite(int *argc, char **argv);
    ~TestSuite();

    int run();
    void onCrashOrTimeout(TestResultType crashOrTimeout);

  private:
    bool parseSingleArg(const char *argument);
    bool launchChildTestProcess(const std::vector<TestIdentifier> &testsInBatch);
    bool finishProcess(ProcessInfo *processInfo);
    int printFailuresAndReturnCount() const;
    void startWatchdog();

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
    int mBatchSize;
    int mCurrentResultCount;
    int mTotalResultCount;
    int mMaxProcesses;
    int mTestTimeout;
    int mBatchTimeout;
    std::vector<std::string> mGoogleTestCommandLineArgs;
    std::map<TestIdentifier, FileLine> mTestFileLines;
    std::vector<ProcessInfo> mCurrentProcesses;
    std::thread mWatchdogThread;
};

bool GetTestResultsFromFile(const char *fileName, TestResults *resultsOut);
}  // namespace angle
