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
 * \brief Merge two test logs.
 *
 * \todo [2013-11-08 pyry] Write variant that can operate with less memory.
 *//*--------------------------------------------------------------------*/

#include "xeTestLogParser.hpp"
#include "xeTestResultParser.hpp"
#include "xeTestLogWriter.hpp"
#include "deString.h"

#include <vector>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>

using std::map;
using std::set;
using std::string;
using std::vector;

enum Flags
{
    FLAG_USE_LAST_INFO = (1 << 0)
};

struct CommandLine
{
    CommandLine(void) : flags(0)
    {
    }

    vector<string> srcFilenames;
    string dstFilename;
    uint32_t flags;
};

class LogHandler : public xe::TestLogHandler
{
public:
    LogHandler(xe::BatchResult *batchResult, uint32_t flags) : m_batchResult(batchResult), m_flags(flags)
    {
    }

    void setSessionInfo(const xe::SessionInfo &info)
    {
        xe::SessionInfo &combinedInfo = m_batchResult->getSessionInfo();

        if (m_flags & FLAG_USE_LAST_INFO)
        {
            if (!info.targetName.empty())
                combinedInfo.targetName = info.targetName;
            if (!info.releaseId.empty())
                combinedInfo.releaseId = info.releaseId;
            if (!info.releaseName.empty())
                combinedInfo.releaseName = info.releaseName;
            if (!info.candyTargetName.empty())
                combinedInfo.candyTargetName = info.candyTargetName;
            if (!info.configName.empty())
                combinedInfo.configName = info.configName;
            if (!info.resultName.empty())
                combinedInfo.resultName = info.resultName;
            if (!info.timestamp.empty())
                combinedInfo.timestamp = info.timestamp;
        }
        else
        {
            if (combinedInfo.targetName.empty())
                combinedInfo.targetName = info.targetName;
            if (combinedInfo.releaseId.empty())
                combinedInfo.releaseId = info.releaseId;
            if (combinedInfo.releaseName.empty())
                combinedInfo.releaseName = info.releaseName;
            if (combinedInfo.candyTargetName.empty())
                combinedInfo.candyTargetName = info.candyTargetName;
            if (combinedInfo.configName.empty())
                combinedInfo.configName = info.configName;
            if (combinedInfo.resultName.empty())
                combinedInfo.resultName = info.resultName;
            if (combinedInfo.timestamp.empty())
                combinedInfo.timestamp = info.timestamp;
        }
    }

    xe::TestCaseResultPtr startTestCaseResult(const char *casePath)
    {
        if (m_batchResult->hasTestCaseResult(casePath))
        {
            xe::TestCaseResultPtr existingResult = m_batchResult->getTestCaseResult(casePath);
            existingResult->clear();
            return existingResult;
        }
        else
            return m_batchResult->createTestCaseResult(casePath);
    }

    void testCaseResultUpdated(const xe::TestCaseResultPtr &)
    {
        // Ignored.
    }

    void testCaseResultComplete(const xe::TestCaseResultPtr &)
    {
        // Ignored.
    }

private:
    xe::BatchResult *const m_batchResult;
    const uint32_t m_flags;
};

static void readLogFile(xe::BatchResult *dstResult, const char *filename, uint32_t flags)
{
    std::ifstream in(filename, std::ifstream::binary | std::ifstream::in);
    LogHandler resultHandler(dstResult, flags);
    xe::TestLogParser parser(&resultHandler);
    uint8_t buf[2048];
    int numRead = 0;

    if (!in.good())
        throw std::runtime_error(string("Failed to open '") + filename + "'");

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

static void mergeTestLogs(const CommandLine &cmdLine)
{
    xe::BatchResult batchResult;

    for (vector<string>::const_iterator filename = cmdLine.srcFilenames.begin(); filename != cmdLine.srcFilenames.end();
         ++filename)
        readLogFile(&batchResult, filename->c_str(), cmdLine.flags);

    if (!cmdLine.dstFilename.empty())
        xe::writeBatchResultToFile(batchResult, cmdLine.dstFilename.c_str());
    else
        xe::writeTestLog(batchResult, std::cout);
}

static void printHelp(const char *binName)
{
    printf("%s: [filename] [[filename 2] ...]\n", binName);
    printf("  --dst=[filename]    Write final log to file, otherwise written to stdout.\n");
    printf("  --info=[first|last] Select which session info to use (default: first).\n");
}

static bool parseCommandLine(CommandLine &cmdLine, int argc, const char *const *argv)
{
    for (int argNdx = 1; argNdx < argc; argNdx++)
    {
        const char *arg = argv[argNdx];

        if (!deStringBeginsWith(arg, "--"))
            cmdLine.srcFilenames.push_back(arg);
        else if (deStringBeginsWith(arg, "--dst="))
        {
            if (!cmdLine.dstFilename.empty())
                return false;
            cmdLine.dstFilename = arg + 6;
        }
        else if (strcmp(arg, "--info=first") == 0)
            cmdLine.flags &= ~FLAG_USE_LAST_INFO;
        else if (strcmp(arg, "--info=last") == 0)
            cmdLine.flags |= FLAG_USE_LAST_INFO;
        else
            return false;
    }

    if (cmdLine.srcFilenames.empty())
        return false;

    return true;
}

int main(int argc, const char *const *argv)
{
    try
    {
        CommandLine cmdLine;

        if (!parseCommandLine(cmdLine, argc, argv))
        {
            printHelp(argv[0]);
            return -1;
        }

        mergeTestLogs(cmdLine);
    }
    catch (const std::exception &e)
    {
        printf("FATAL ERROR: %s\n", e.what());
        return -1;
    }

    return 0;
}
