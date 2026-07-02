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
 * \brief Test log compare utility.
 *//*--------------------------------------------------------------------*/

#include "xeTestLogParser.hpp"
#include "xeTestResultParser.hpp"
#include "deFilePath.hpp"
#include "deString.h"
#include "deThread.hpp"
#include "deCommandLine.hpp"

#include <vector>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <set>
#include <map>

using std::map;
using std::set;
using std::string;
using std::vector;

enum OutputMode
{
    OUTPUTMODE_ALL = 0,
    OUTPUTMODE_DIFF,

    OUTPUTMODE_LAST
};

enum OutputFormat
{
    OUTPUTFORMAT_TEXT = 0,
    OUTPUTFORMAT_CSV,

    OUTPUTFORMAT_LAST
};

enum OutputValue
{
    OUTPUTVALUE_STATUS_CODE = 0,
    OUTPUTVALUE_STATUS_DETAILS,

    OUTPUTVALUE_LAST
};

namespace opt
{

DE_DECLARE_COMMAND_LINE_OPT(OutMode, OutputMode);
DE_DECLARE_COMMAND_LINE_OPT(OutFormat, OutputFormat);
DE_DECLARE_COMMAND_LINE_OPT(OutValue, OutputValue);

static void registerOptions(de::cmdline::Parser &parser)
{
    using de::cmdline::NamedValue;
    using de::cmdline::Option;

    static const NamedValue<OutputMode> s_outputModes[]     = {{"all", OUTPUTMODE_ALL}, {"diff", OUTPUTMODE_DIFF}};
    static const NamedValue<OutputFormat> s_outputFormats[] = {{"text", OUTPUTFORMAT_TEXT}, {"csv", OUTPUTFORMAT_CSV}};
    static const NamedValue<OutputValue> s_outputValues[]   = {{"code", OUTPUTVALUE_STATUS_CODE},
                                                               {"details", OUTPUTVALUE_STATUS_DETAILS}};

    parser << Option<OutFormat>("f", "format", "Output format", s_outputFormats, "csv")
           << Option<OutMode>("m", "mode", "Output mode", s_outputModes, "all")
           << Option<OutValue>("v", "value", "Value to extract", s_outputValues, "code");
}

} // namespace opt

struct CommandLine
{
    CommandLine(void) : outMode(OUTPUTMODE_ALL), outFormat(OUTPUTFORMAT_CSV), outValue(OUTPUTVALUE_STATUS_CODE)
    {
    }

    OutputMode outMode;
    OutputFormat outFormat;
    OutputValue outValue;
    vector<string> filenames;
};

struct ShortBatchResult
{
    vector<xe::TestCaseResultHeader> resultHeaders;
    map<string, int> resultMap;
};

class ShortResultHandler : public xe::TestLogHandler
{
public:
    ShortResultHandler(ShortBatchResult &result) : m_result(result)
    {
    }

    void setSessionInfo(const xe::SessionInfo &)
    {
        // Ignored.
    }

    xe::TestCaseResultPtr startTestCaseResult(const char *casePath)
    {
        return xe::TestCaseResultPtr(new xe::TestCaseResultData(casePath));
    }

    void testCaseResultUpdated(const xe::TestCaseResultPtr &)
    {
        // Ignored.
    }

    void testCaseResultComplete(const xe::TestCaseResultPtr &caseData)
    {
        xe::TestCaseResultHeader header;
        int caseNdx = (int)m_result.resultHeaders.size();

        header.casePath      = caseData->getTestCasePath();
        header.caseType      = xe::TESTCASETYPE_SELF_VALIDATE;
        header.statusCode    = caseData->getStatusCode();
        header.statusDetails = caseData->getStatusDetails();

        if (header.statusCode == xe::TESTSTATUSCODE_LAST)
        {
            xe::TestCaseResult fullResult;

            xe::parseTestCaseResultFromData(&m_testResultParser, &fullResult, *caseData.get());

            header = xe::TestCaseResultHeader(fullResult);
        }

        // Insert into result list & map.
        m_result.resultHeaders.push_back(header);
        m_result.resultMap[header.casePath] = caseNdx;
    }

private:
    ShortBatchResult &m_result;
    xe::TestResultParser m_testResultParser;
};

static void readLogFile(ShortBatchResult &batchResult, const char *filename)
{
    std::ifstream in(filename, std::ifstream::binary | std::ifstream::in);
    ShortResultHandler resultHandler(batchResult);
    xe::TestLogParser parser(&resultHandler);
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

class LogFileReader : public de::Thread
{
public:
    LogFileReader(ShortBatchResult &batchResult, const char *filename)
        : m_batchResult(batchResult)
        , m_filename(filename)
    {
    }

    void run(void)
    {
        readLogFile(m_batchResult, m_filename.c_str());
    }

private:
    ShortBatchResult &m_batchResult;
    std::string m_filename;
};

static void computeCaseList(vector<string> &cases, const vector<ShortBatchResult> &batchResults)
{
    // \todo [2012-07-10 pyry] Do proper case ordering (eg. handle missing cases nicely).
    set<string> addedCases;

    for (vector<ShortBatchResult>::const_iterator batchIter = batchResults.begin(); batchIter != batchResults.end();
         batchIter++)
    {
        for (vector<xe::TestCaseResultHeader>::const_iterator caseIter = batchIter->resultHeaders.begin();
             caseIter != batchIter->resultHeaders.end(); caseIter++)
        {
            if (addedCases.find(caseIter->casePath) == addedCases.end())
            {
                cases.push_back(caseIter->casePath);
                addedCases.insert(caseIter->casePath);
            }
        }
    }
}

static void getTestResultHeaders(vector<xe::TestCaseResultHeader> &headers,
                                 const vector<ShortBatchResult> &batchResults, const char *casePath)
{
    headers.resize(batchResults.size());

    for (int ndx = 0; ndx < (int)batchResults.size(); ndx++)
    {
        const ShortBatchResult &batchResult        = batchResults[ndx];
        map<string, int>::const_iterator resultPos = batchResult.resultMap.find(casePath);

        if (resultPos != batchResult.resultMap.end())
            headers[ndx] = batchResult.resultHeaders[resultPos->second];
        else
        {
            headers[ndx].casePath   = casePath;
            headers[ndx].caseType   = xe::TESTCASETYPE_SELF_VALIDATE;
            headers[ndx].statusCode = xe::TESTSTATUSCODE_LAST;
        }
    }
}

static const char *getStatusCodeName(xe::TestStatusCode code)
{
    if (code == xe::TESTSTATUSCODE_LAST)
        return "Missing";
    else
        return xe::getTestStatusCodeName(code);
}

static bool runCompare(const CommandLine &cmdLine, std::ostream &dst)
{
    vector<ShortBatchResult> results;
    vector<string> batchNames;
    bool compareOk = true;

    XE_CHECK(!cmdLine.filenames.empty());

    try
    {
        // Read in batch results
        results.resize(cmdLine.filenames.size());
        {
            std::vector<de::SharedPtr<LogFileReader>> readers;

            for (int ndx = 0; ndx < (int)cmdLine.filenames.size(); ndx++)
            {
                readers.push_back(
                    de::SharedPtr<LogFileReader>(new LogFileReader(results[ndx], cmdLine.filenames[ndx].c_str())));
                readers.back()->start();
            }

            for (int ndx = 0; ndx < (int)cmdLine.filenames.size(); ndx++)
            {
                readers[ndx]->join();

                // Use file name as batch name.
                batchNames.push_back(de::FilePath(cmdLine.filenames[ndx].c_str()).getBaseName());
            }
        }

        // Compute unified case list.
        vector<string> caseList;
        computeCaseList(caseList, results);

        // Stats.
        int numCases = (int)caseList.size();
        int numEqual = 0;

        if (cmdLine.outFormat == OUTPUTFORMAT_CSV)
        {
            dst << "TestCasePath";
            for (vector<string>::const_iterator nameIter = batchNames.begin(); nameIter != batchNames.end(); nameIter++)
                dst << "," << *nameIter;
            dst << "\n";
        }

        // Compare cases.
        for (vector<string>::const_iterator caseIter = caseList.begin(); caseIter != caseList.end(); caseIter++)
        {
            const string &caseName = *caseIter;
            vector<xe::TestCaseResultHeader> headers;
            bool allEqual = true;

            getTestResultHeaders(headers, results, caseName.c_str());

            for (vector<xe::TestCaseResultHeader>::const_iterator iter = headers.begin() + 1; iter != headers.end();
                 iter++)
            {
                if (iter->statusCode != headers[0].statusCode)
                {
                    allEqual = false;
                    break;
                }
            }

            if (allEqual)
                numEqual += 1;

            if (cmdLine.outMode == OUTPUTMODE_ALL || !allEqual)
            {
                if (cmdLine.outFormat == OUTPUTFORMAT_TEXT)
                {
                    dst << caseName << "\n";
                    for (int ndx = 0; ndx < (int)headers.size(); ndx++)
                        dst << "  " << batchNames[ndx] << ": " << getStatusCodeName(headers[ndx].statusCode) << " ("
                            << headers[ndx].statusDetails << ")\n";
                    dst << "\n";
                }
                else if (cmdLine.outFormat == OUTPUTFORMAT_CSV)
                {
                    dst << caseName;
                    for (vector<xe::TestCaseResultHeader>::const_iterator iter = headers.begin(); iter != headers.end();
                         iter++)
                        dst << ","
                            << (cmdLine.outValue == OUTPUTVALUE_STATUS_CODE ? getStatusCodeName(iter->statusCode) :
                                                                              iter->statusDetails.c_str());
                    dst << "\n";
                }
            }
        }

        compareOk = numEqual == numCases;

        if (cmdLine.outFormat == OUTPUTFORMAT_TEXT)
        {
            dst << "  " << numEqual << " / " << numCases << " test case results match.\n";
            dst << "  Comparison " << (compareOk ? "passed" : "FAILED") << "!\n";
        }
    }
    catch (const std::exception &e)
    {
        printf("%s\n", e.what());
        compareOk = false;
    }

    return compareOk;
}

static bool parseCommandLine(CommandLine &cmdLine, int argc, const char *const *argv)
{
    de::cmdline::Parser parser;
    de::cmdline::CommandLine opts;

    XE_CHECK(argc >= 1);

    opt::registerOptions(parser);

    if (!parser.parse(argc - 1, &argv[1], &opts, std::cerr) || opts.getArgs().empty())
    {
        std::cout << argv[0] << ": [options] [filenames]\n";
        parser.help(std::cout);
        return false;
    }

    cmdLine.outFormat = opts.getOption<opt::OutFormat>();
    cmdLine.outMode   = opts.getOption<opt::OutMode>();
    cmdLine.outValue  = opts.getOption<opt::OutValue>();
    cmdLine.filenames = opts.getArgs();

    return true;
}

int main(int argc, const char *const *argv)
{
    CommandLine cmdLine;

    if (!parseCommandLine(cmdLine, argc, argv))
        return -1;

    try
    {
        bool compareOk = runCompare(cmdLine, std::cout);
        return compareOk ? 0 : -1;
    }
    catch (const std::exception &e)
    {
        printf("FATAL ERROR: %s\n", e.what());
        return -1;
    }
}
