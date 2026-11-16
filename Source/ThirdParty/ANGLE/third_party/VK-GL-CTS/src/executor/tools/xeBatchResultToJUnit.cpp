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
 * \brief Batch result to JUnit report conversion tool.
 *//*--------------------------------------------------------------------*/

#include "xeTestLogParser.hpp"
#include "xeTestResultParser.hpp"
#include "xeXMLWriter.hpp"
#include "deFilePath.hpp"
#include "deString.h"
#include "deStringUtil.hpp"

#include <vector>
#include <string>
#include <map>
#include <cstdio>
#include <fstream>

using std::map;
using std::string;
using std::vector;

struct CommandLine
{
    CommandLine(void)
    {
    }

    std::string batchResultFile;
    std::string outputFile;
};

static void printHelp(const char *binName)
{
    printf("%s: [testlog] [output file]\n", binName);
}

static void parseCommandLine(CommandLine &cmdLine, int argc, const char *const *argv)
{
    if (argc != 3)
        throw xe::Error("Expected input and output paths");

    cmdLine.batchResultFile = argv[argc - 2];
    cmdLine.outputFile      = argv[argc - 1];
}

static void parseBatchResult(xe::TestLogParser &parser, const char *filename)
{
    std::ifstream in(filename, std::ios_base::binary);
    uint8_t buf[2048];

    for (;;)
    {
        in.read((char *)&buf[0], sizeof(buf));
        int numRead = (int)in.gcount();

        if (numRead > 0)
            parser.parse(&buf[0], numRead);

        if (numRead < (int)sizeof(buf))
            break;
    }
}

class ResultToJUnitHandler : public xe::TestLogHandler
{
public:
    ResultToJUnitHandler(xe::xml::Writer &writer) : m_writer(writer)
    {
    }

    void setSessionInfo(const xe::SessionInfo &)
    {
    }

    xe::TestCaseResultPtr startTestCaseResult(const char *casePath)
    {
        return xe::TestCaseResultPtr(new xe::TestCaseResultData(casePath));
    }

    void testCaseResultUpdated(const xe::TestCaseResultPtr &)
    {
    }

    void testCaseResultComplete(const xe::TestCaseResultPtr &resultData)
    {
        using xe::xml::Writer;

        xe::TestCaseResult result;

        xe::parseTestCaseResultFromData(&m_resultParser, &result, *resultData.get());

        // Split group and case names.
        size_t sepPos         = result.casePath.find_last_of('.');
        std::string caseName  = result.casePath.substr(sepPos + 1);
        std::string groupName = result.casePath.substr(0, sepPos);

        // Write result.
        m_writer << Writer::BeginElement("testcase") << Writer::Attribute("name", caseName)
                 << Writer::Attribute("classname", groupName);

        if (result.statusCode != xe::TESTSTATUSCODE_PASS)
            m_writer << Writer::BeginElement("failure")
                     << Writer::Attribute("type", xe::getTestStatusCodeName(result.statusCode)) << result.statusDetails
                     << Writer::EndElement;

        m_writer << Writer::EndElement;
    }

private:
    xe::xml::Writer &m_writer;
    xe::TestResultParser m_resultParser;
};

static void batchResultToJUnitReport(const char *batchResultFilename, const char *dstFileName)
{
    std::ofstream out(dstFileName, std::ios_base::binary);
    xe::xml::Writer writer(out);
    ResultToJUnitHandler handler(writer);
    xe::TestLogParser parser(&handler);

    XE_CHECK(out.good());

    out << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";

    writer << xe::xml::Writer::BeginElement("testsuites") << xe::xml::Writer::BeginElement("testsuite");

    // Parse and write individual cases
    parseBatchResult(parser, batchResultFilename);

    writer << xe::xml::Writer::EndElement << xe::xml::Writer::EndElement;
}

int main(int argc, const char *const *argv)
{
    CommandLine cmdLine;
    try
    {
        parseCommandLine(cmdLine, argc, argv);
    }
    catch (const std::exception &)
    {
        printHelp(argv[0]);
        return -1;
    }

    try
    {
        batchResultToJUnitReport(cmdLine.batchResultFile.c_str(), cmdLine.outputFile.c_str());
    }
    catch (const std::exception &e)
    {
        printf("%s\n", e.what());
        return -1;
    }

    return 0;
}
