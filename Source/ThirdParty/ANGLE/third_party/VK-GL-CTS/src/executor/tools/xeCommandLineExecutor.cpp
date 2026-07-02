/*-------------------------------------------------------------------------
 * drawElements Quality Program Test Executor
 * ------------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Command line test executor.
 *//*--------------------------------------------------------------------*/

#include "xeBatchExecutor.hpp"
#include "xeLocalTcpIpLink.hpp"
#include "xeTcpIpLink.hpp"
#include "xeTestCaseListParser.hpp"
#include "xeTestLogWriter.hpp"
#include "xeTestResultParser.hpp"

#include "deCommandLine.hpp"
#include "deDirectoryIterator.hpp"
#include "deStringUtil.hpp"
#include "deUniquePtr.hpp"

#include "deString.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#if (DE_OS == DE_OS_UNIX) || (DE_OS == DE_OS_ANDROID) || (DE_OS == DE_OS_WIN32)
#include <signal.h>
#endif

using std::string;
using std::vector;

namespace
{

// Command line arguments.
namespace opt
{

DE_DECLARE_COMMAND_LINE_OPT(StartServer, string);
DE_DECLARE_COMMAND_LINE_OPT(Host, string);
DE_DECLARE_COMMAND_LINE_OPT(Port, int);
DE_DECLARE_COMMAND_LINE_OPT(CaseListDir, string);
DE_DECLARE_COMMAND_LINE_OPT(TestSet, vector<string>);
DE_DECLARE_COMMAND_LINE_OPT(ExcludeSet, vector<string>);
DE_DECLARE_COMMAND_LINE_OPT(ContinueFile, string);
DE_DECLARE_COMMAND_LINE_OPT(TestLogFile, string);
DE_DECLARE_COMMAND_LINE_OPT(InfoLogFile, string);
DE_DECLARE_COMMAND_LINE_OPT(Summary, bool);

// TargetConfiguration
DE_DECLARE_COMMAND_LINE_OPT(BinaryName, string);
DE_DECLARE_COMMAND_LINE_OPT(WorkingDir, string);
DE_DECLARE_COMMAND_LINE_OPT(CmdLineArgs, string);

void parseCommaSeparatedList(const char *src, vector<string> *dst)
{
    std::istringstream inStr(src);
    string comp;

    while (std::getline(inStr, comp, ','))
        dst->push_back(comp);
}

void registerOptions(de::cmdline::Parser &parser)
{
    using de::cmdline::NamedValue;
    using de::cmdline::Option;

    static const NamedValue<bool> s_yesNo[] = {{"yes", true}, {"no", false}};

    parser << Option<StartServer>("s", "start-server", "Start local execserver. Path to the execserver binary.")
           << Option<Host>("c", "connect", "Connect to host. Address of the execserver.")
           << Option<Port>("p", "port", "TCP port of the execserver.", "50016")
           << Option<CaseListDir>("cd", "caselistdir", "Path to the directory containing test case XML files.", ".")
           << Option<TestSet>("t", "testset", "Comma-separated list of include filters.", parseCommaSeparatedList)
           << Option<ExcludeSet>("e", "exclude", "Comma-separated list of exclude filters.", parseCommaSeparatedList,
                                 "")
           << Option<ContinueFile>(nullptr, "continue",
                                   "Continue execution by initializing results from existing test log.")
           << Option<TestLogFile>("o", "out", "Output test log filename.", "TestLog.qpa")
           << Option<InfoLogFile>("i", "info", "Output info log filename.", "InfoLog.txt")
           << Option<Summary>(nullptr, "summary", "Print summary after running tests.", s_yesNo, "yes")
           << Option<BinaryName>("b", "binaryname", "Test binary path. Relative to working directory.", "<Unused>")
           << Option<WorkingDir>("wd", "workdir", "Working directory for the test execution.", ".")
           << Option<CmdLineArgs>(nullptr, "cmdline", "Additional command line arguments for the test binary.", "");
}

} // namespace opt

enum RunMode
{
    RUNMODE_CONNECT,
    RUNMODE_START_SERVER
};

struct CommandLine
{
    CommandLine(void) : port(0), summary(false)
    {
    }

    xe::TargetConfiguration targetCfg;
    RunMode runMode;
    string serverBinOrAddress;
    int port;
    string caseListDir;
    vector<string> testset;
    vector<string> exclude;
    string inFile;
    string outFile;
    string infoFile;
    bool summary;
};

bool parseCommandLine(CommandLine &cmdLine, int argc, const char *const *argv)
{
    de::cmdline::Parser parser;
    de::cmdline::CommandLine opts;

    XE_CHECK(argc >= 1);

    opt::registerOptions(parser);

    if (!parser.parse(argc - 1, argv + 1, &opts, std::cerr))
    {
        std::cout << argv[0] << " [options]\n";
        parser.help(std::cout);
        return false;
    }

    if (opts.hasOption<opt::StartServer>() && opts.hasOption<opt::Host>())
    {
        std::cout << "Invalid command line arguments. Both --start-server and --connect defined." << std::endl;
        return false;
    }
    else if (!opts.hasOption<opt::StartServer>() && !opts.hasOption<opt::Host>())
    {
        std::cout << "Invalid command line arguments. Must define either --start-server or --connect." << std::endl;
        return false;
    }

    if (!opts.hasOption<opt::TestSet>())
    {
        std::cout << "Invalid command line arguments. --testset not defined." << std::endl;
        return false;
    }

    if (opts.hasOption<opt::StartServer>())
    {
        cmdLine.runMode            = RUNMODE_START_SERVER;
        cmdLine.serverBinOrAddress = opts.getOption<opt::StartServer>();
    }
    else
    {
        cmdLine.runMode            = RUNMODE_CONNECT;
        cmdLine.serverBinOrAddress = opts.getOption<opt::Host>();
    }

    if (opts.hasOption<opt::ContinueFile>())
    {
        cmdLine.inFile = opts.getOption<opt::ContinueFile>();

        if (cmdLine.inFile.empty())
        {
            std::cout << "Invalid command line arguments. --continue argument is empty." << std::endl;
            return false;
        }
    }

    cmdLine.port                  = opts.getOption<opt::Port>();
    cmdLine.caseListDir           = opts.getOption<opt::CaseListDir>();
    cmdLine.testset               = opts.getOption<opt::TestSet>();
    cmdLine.exclude               = opts.getOption<opt::ExcludeSet>();
    cmdLine.outFile               = opts.getOption<opt::TestLogFile>();
    cmdLine.infoFile              = opts.getOption<opt::InfoLogFile>();
    cmdLine.summary               = opts.getOption<opt::Summary>();
    cmdLine.targetCfg.binaryName  = opts.getOption<opt::BinaryName>();
    cmdLine.targetCfg.workingDir  = opts.getOption<opt::WorkingDir>();
    cmdLine.targetCfg.cmdLineArgs = opts.getOption<opt::CmdLineArgs>();

    return true;
}

bool checkCasePathPatternMatch(const char *pattern, const char *casePath, bool isTestGroup)
{
    int ptrnPos = 0;
    int casePos = 0;

    for (;;)
    {
        char c = casePath[casePos];
        char p = pattern[ptrnPos];

        if (p == '*')
        {
            /* Recurse to rest of positions. */
            int next = casePos;
            for (;;)
            {
                if (checkCasePathPatternMatch(pattern + ptrnPos + 1, casePath + next, isTestGroup))
                    return true;

                if (casePath[next] == 0)
                    return false; /* No match found. */
                else
                    next += 1;
            }
            DE_ASSERT(false);
        }
        else if (c == 0 && p == 0)
            return true;
        else if (c == 0)
        {
            /* Incomplete match is ok for test groups. */
            return isTestGroup;
        }
        else if (c != p)
            return false;

        casePos += 1;
        ptrnPos += 1;
    }

    DE_ASSERT(false);
    return false;
}

void readCaseList(xe::TestGroup *root, const char *filename)
{
    xe::TestCaseListParser caseListParser;
    std::ifstream in(filename, std::ios_base::binary);
    uint8_t buf[1024];

    XE_CHECK(in.good());

    caseListParser.init(root);

    for (;;)
    {
        in.read((char *)&buf[0], sizeof(buf));
        int numRead = (int)in.gcount();

        if (numRead > 0)
            caseListParser.parse(&buf[0], numRead);

        if (numRead < (int)sizeof(buf))
            break; // EOF
    }
}

void readCaseLists(xe::TestRoot &root, const char *caseListDir)
{
    int testCaseListCount = 0;
    de::DirectoryIterator iter(caseListDir);

    for (; iter.hasItem(); iter.next())
    {
        de::FilePath item = iter.getItem();

        if (item.getType() == de::FilePath::TYPE_FILE)
        {
            string baseName = item.getBaseName();
            if (baseName.find("-cases.xml") == baseName.length() - 10)
            {
                string packageName     = baseName.substr(0, baseName.length() - 10);
                xe::TestGroup *package = root.createGroup(packageName.c_str());

                readCaseList(package, item.getPath());
                testCaseListCount++;
            }
        }
    }

    if (testCaseListCount == 0)
        throw xe::Error("Couldn't find test case lists from test case list directory: '" + string(caseListDir) + "'");
}

void addMatchingCases(const xe::TestGroup &group, xe::TestSet &testSet, const char *filter)
{
    for (int childNdx = 0; childNdx < group.getNumChildren(); childNdx++)
    {
        const xe::TestNode *child = group.getChild(childNdx);
        const bool isGroup        = child->getNodeType() == xe::TESTNODETYPE_GROUP;
        const string fullPath     = child->getFullPath();

        if (checkCasePathPatternMatch(filter, fullPath.c_str(), isGroup))
        {
            if (isGroup)
            {
                // Recurse into group.
                addMatchingCases(static_cast<const xe::TestGroup &>(*child), testSet, filter);
            }
            else
            {
                DE_ASSERT(child->getNodeType() == xe::TESTNODETYPE_TEST_CASE);
                testSet.add(child);
            }
        }
    }
}

void removeMatchingCases(const xe::TestGroup &group, xe::TestSet &testSet, const char *filter)
{
    for (int childNdx = 0; childNdx < group.getNumChildren(); childNdx++)
    {
        const xe::TestNode *child = group.getChild(childNdx);
        const bool isGroup        = child->getNodeType() == xe::TESTNODETYPE_GROUP;
        const string fullPath     = child->getFullPath();

        if (checkCasePathPatternMatch(filter, fullPath.c_str(), isGroup))
        {
            if (isGroup)
            {
                // Recurse into group.
                removeMatchingCases(static_cast<const xe::TestGroup &>(*child), testSet, filter);
            }
            else
            {
                DE_ASSERT(child->getNodeType() == xe::TESTNODETYPE_TEST_CASE);
                testSet.remove(child);
            }
        }
    }
}

class BatchResultHandler : public xe::TestLogHandler
{
public:
    BatchResultHandler(xe::BatchResult *batchResult) : m_batchResult(batchResult)
    {
    }

    void setSessionInfo(const xe::SessionInfo &sessionInfo)
    {
        m_batchResult->getSessionInfo() = sessionInfo;
    }

    xe::TestCaseResultPtr startTestCaseResult(const char *casePath)
    {
        // \todo [2012-11-01 pyry] What to do with duplicate results?
        if (m_batchResult->hasTestCaseResult(casePath))
            return m_batchResult->getTestCaseResult(casePath);
        else
            return m_batchResult->createTestCaseResult(casePath);
    }

    void testCaseResultUpdated(const xe::TestCaseResultPtr &)
    {
    }

    void testCaseResultComplete(const xe::TestCaseResultPtr &)
    {
    }

private:
    xe::BatchResult *m_batchResult;
};

void readLogFile(xe::BatchResult *batchResult, const char *filename)
{
    std::ifstream in(filename, std::ifstream::binary | std::ifstream::in);
    BatchResultHandler handler(batchResult);
    xe::TestLogParser parser(&handler);
    uint8_t buf[1024];
    int numRead = 0;

    for (;;)
    {
        in.read((char *)&buf[0], DE_LENGTH_OF_ARRAY(buf));
        numRead = (int)in.gcount();

        if (numRead <= 0)
            break;

        parser.parse(&buf[0], numRead);
    }

    in.close();
}

void printBatchResultSummary(const xe::TestNode *root, const xe::TestSet &testSet, const xe::BatchResult &batchResult)
{
    int countByStatusCode[xe::TESTSTATUSCODE_LAST];
    std::fill(&countByStatusCode[0], &countByStatusCode[0] + DE_LENGTH_OF_ARRAY(countByStatusCode), 0);

    for (xe::ConstTestNodeIterator iter = xe::ConstTestNodeIterator::begin(root);
         iter != xe::ConstTestNodeIterator::end(root); ++iter)
    {
        const xe::TestNode *node = *iter;
        if (node->getNodeType() == xe::TESTNODETYPE_TEST_CASE && testSet.hasNode(node))
        {
            const xe::TestCase *testCase = static_cast<const xe::TestCase *>(node);
            string fullPath;
            xe::TestStatusCode statusCode = xe::TESTSTATUSCODE_PENDING;
            testCase->getFullPath(fullPath);

            // Parse result data if such exists.
            if (batchResult.hasTestCaseResult(fullPath.c_str()))
            {
                xe::ConstTestCaseResultPtr resultData = batchResult.getTestCaseResult(fullPath.c_str());
                xe::TestCaseResult result;
                xe::TestResultParser parser;

                xe::parseTestCaseResultFromData(&parser, &result, *resultData.get());
                statusCode = result.statusCode;
            }

            countByStatusCode[statusCode] += 1;
        }
    }

    printf("\nTest run summary:\n");
    int totalCases = 0;
    for (int code = 0; code < xe::TESTSTATUSCODE_LAST; code++)
    {
        if (countByStatusCode[code] > 0)
            printf("  %20s: %5d\n", xe::getTestStatusCodeName((xe::TestStatusCode)code), countByStatusCode[code]);

        totalCases += countByStatusCode[code];
    }
    printf("  %20s: %5d\n", "Total", totalCases);
}

void writeInfoLog(const xe::InfoLog &log, const char *filename)
{
    std::ofstream out(filename, std::ios_base::binary);
    XE_CHECK(out.good());
    out.write((const char *)log.getBytes(), log.getSize());
    out.close();
}

xe::CommLink *createCommLink(const CommandLine &cmdLine)
{
    if (cmdLine.runMode == RUNMODE_START_SERVER)
    {
        xe::LocalTcpIpLink *link = new xe::LocalTcpIpLink();
        try
        {
            link->start(cmdLine.serverBinOrAddress.c_str(), nullptr, cmdLine.port);
            return link;
        }
        catch (...)
        {
            delete link;
            throw;
        }
    }
    else if (cmdLine.runMode == RUNMODE_CONNECT)
    {
        de::SocketAddress address;

        address.setFamily(DE_SOCKETFAMILY_INET4);
        address.setProtocol(DE_SOCKETPROTOCOL_TCP);
        address.setHost(cmdLine.serverBinOrAddress.c_str());
        address.setPort(cmdLine.port);

        xe::TcpIpLink *link = new xe::TcpIpLink();
        try
        {
            std::string error;

            link->connect(address);
            return link;
        }
        catch (const std::exception &error)
        {
            delete link;
            throw xe::Error("Failed to connect to ExecServer at: " + cmdLine.serverBinOrAddress + ":" +
                            de::toString(cmdLine.port) + ", " + error.what());
        }
        catch (...)
        {
            delete link;
            throw;
        }
    }
    else
    {
        DE_ASSERT(false);
        return nullptr;
    }
}

#if (DE_OS == DE_OS_UNIX) || (DE_OS == DE_OS_ANDROID)

static xe::BatchExecutor *s_executor = nullptr;

void signalHandler(int, siginfo_t *, void *)
{
    if (s_executor)
        s_executor->cancel();
}

void setupSignalHandler(xe::BatchExecutor *executor)
{
    s_executor = executor;
    struct sigaction sa;

    sa.sa_sigaction = signalHandler;
    sa.sa_flags     = SA_SIGINFO | SA_RESTART;
    sigfillset(&sa.sa_mask);

    sigaction(SIGINT, &sa, nullptr);
}

void resetSignalHandler(void)
{
    struct sigaction sa;

    sa.sa_handler = SIG_DFL;
    sa.sa_flags   = SA_RESTART;
    sigfillset(&sa.sa_mask);

    sigaction(SIGINT, &sa, nullptr);
    s_executor = nullptr;
}

#elif (DE_OS == DE_OS_WIN32)

static xe::BatchExecutor *s_executor = nullptr;

void signalHandler(int)
{
    if (s_executor)
        s_executor->cancel();
}

void setupSignalHandler(xe::BatchExecutor *executor)
{
    s_executor = executor;
    signal(SIGINT, signalHandler);
}

void resetSignalHandler(void)
{
    signal(SIGINT, SIG_DFL);
    s_executor = nullptr;
}

#else

void setupSignalHandler(xe::BatchExecutor *)
{
}

void resetSignalHandler(void)
{
}

#endif

void runExecutor(const CommandLine &cmdLine)
{
    xe::TestRoot root;

    // Read case list definitions.
    readCaseLists(root, cmdLine.caseListDir.c_str());

    // Build test set.
    xe::TestSet testSet;

    // Build test set.
    for (vector<string>::const_iterator filterIter = cmdLine.testset.begin(); filterIter != cmdLine.testset.end();
         ++filterIter)
        addMatchingCases(root, testSet, filterIter->c_str());

    if (testSet.empty())
        throw xe::Error("None of the test case lists contains tests matching any of the test sets.");

    // Remove excluded cases.
    for (vector<string>::const_iterator filterIter = cmdLine.exclude.begin(); filterIter != cmdLine.exclude.end();
         ++filterIter)
        removeMatchingCases(root, testSet, filterIter->c_str());

    // Initialize batch result.
    xe::BatchResult batchResult;
    xe::InfoLog infoLog;

    // Read existing results from input file (if supplied).
    if (!cmdLine.inFile.empty())
        readLogFile(&batchResult, cmdLine.inFile.c_str());

    // Initialize commLink.
    de::UniquePtr<xe::CommLink> commLink(createCommLink(cmdLine));

    xe::BatchExecutor executor(cmdLine.targetCfg, commLink.get(), &root, testSet, &batchResult, &infoLog);

    try
    {
        setupSignalHandler(&executor);
        executor.run();
        resetSignalHandler();
    }
    catch (...)
    {
        resetSignalHandler();

        if (!cmdLine.outFile.empty())
        {
            xe::writeBatchResultToFile(batchResult, cmdLine.outFile.c_str());
            printf("Test log written to %s\n", cmdLine.outFile.c_str());
        }

        if (!cmdLine.infoFile.empty())
        {
            writeInfoLog(infoLog, cmdLine.infoFile.c_str());
            printf("Info log written to %s\n", cmdLine.infoFile.c_str());
        }

        if (cmdLine.summary)
            printBatchResultSummary(&root, testSet, batchResult);

        throw;
    }

    if (!cmdLine.outFile.empty())
    {
        xe::writeBatchResultToFile(batchResult, cmdLine.outFile.c_str());
        printf("Test log written to %s\n", cmdLine.outFile.c_str());
    }

    if (!cmdLine.infoFile.empty())
    {
        writeInfoLog(infoLog, cmdLine.infoFile.c_str());
        printf("Info log written to %s\n", cmdLine.infoFile.c_str());
    }

    if (cmdLine.summary)
        printBatchResultSummary(&root, testSet, batchResult);

    {
        string err;

        if (commLink->getState(err) == xe::COMMLINKSTATE_ERROR)
            throw xe::Error(err);
    }
}

} // namespace

int main(int argc, const char *const *argv)
{
    CommandLine cmdLine;

    if (!parseCommandLine(cmdLine, argc, argv))
        return -1;

    try
    {
        runExecutor(cmdLine);
    }
    catch (const std::exception &e)
    {
        printf("%s\n", e.what());
        return -1;
    }

    return 0;
}
