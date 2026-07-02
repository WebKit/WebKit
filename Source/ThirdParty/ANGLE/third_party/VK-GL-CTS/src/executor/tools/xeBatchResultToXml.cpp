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
 * \brief Batch result to XML export.
 *//*--------------------------------------------------------------------*/

#include "xeTestLogParser.hpp"
#include "xeTestResultParser.hpp"
#include "xeXMLWriter.hpp"
#include "xeTestLogWriter.hpp"
#include "deFilePath.hpp"
#include "deString.h"
#include "deStringUtil.hpp"
#include "deCommandLine.hpp"

#include <vector>
#include <string>
#include <map>
#include <cstdio>
#include <fstream>
#include <iostream>

using std::map;
using std::string;
using std::vector;

static const char *CASELIST_STYLESHEET = "caselist.xsl";
static const char *TESTCASE_STYLESHEET = "testlog.xsl";

enum OutputMode
{
    OUTPUTMODE_SEPARATE = 0, //!< Separate
    OUTPUTMODE_SINGLE,

    OUTPUTMODE_LAST
};

namespace opt
{

DE_DECLARE_COMMAND_LINE_OPT(OutMode, OutputMode);

void registerOptions(de::cmdline::Parser &parser)
{
    using de::cmdline::NamedValue;
    using de::cmdline::Option;

    static const NamedValue<OutputMode> s_modes[] = {{"single", OUTPUTMODE_SINGLE}, {"separate", OUTPUTMODE_SEPARATE}};

    parser << Option<OutMode>("m", "mode", "Output mode", s_modes, "single");
}

} // namespace opt

struct CommandLine
{
    CommandLine(void) : outputMode(OUTPUTMODE_SINGLE)
    {
    }

    std::string batchResultFile;
    std::string outputPath;
    OutputMode outputMode;
};

static bool parseCommandLine(CommandLine &cmdLine, int argc, const char *const *argv)
{
    de::cmdline::Parser parser;
    de::cmdline::CommandLine opts;

    opt::registerOptions(parser);

    if (!parser.parse(argc - 1, argv + 1, &opts, std::cerr) || opts.getArgs().size() != 2)
    {
        printf("%s: [options] [testlog] [destination path]\n", argv[0]);
        parser.help(std::cout);
        return false;
    }

    cmdLine.outputMode      = opts.getOption<opt::OutMode>();
    cmdLine.batchResultFile = opts.getArgs()[0];
    cmdLine.outputPath      = opts.getArgs()[1];

    return true;
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

// Export to single file

struct BatchResultTotals
{
    BatchResultTotals(void)
    {
        for (int i = 0; i < xe::TESTSTATUSCODE_LAST; i++)
            countByCode[i] = 0;
    }

    int countByCode[xe::TESTSTATUSCODE_LAST];
};

class ResultToSingleXmlLogHandler : public xe::TestLogHandler
{
public:
    ResultToSingleXmlLogHandler(xe::xml::Writer &writer, BatchResultTotals &totals) : m_writer(writer), m_totals(totals)
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
        xe::TestCaseResult result;

        xe::parseTestCaseResultFromData(&m_resultParser, &result, *resultData.get());

        // Write result.
        xe::writeTestResult(result, m_writer);

        // Record total
        XE_CHECK(de::inBounds<int>(result.statusCode, 0, xe::TESTSTATUSCODE_LAST));
        m_totals.countByCode[result.statusCode] += 1;
    }

private:
    xe::xml::Writer &m_writer;
    BatchResultTotals &m_totals;
    xe::TestResultParser m_resultParser;
};

static void writeTotals(xe::xml::Writer &writer, const BatchResultTotals &totals)
{
    using xe::xml::Writer;

    int totalCases = 0;

    writer << Writer::BeginElement("ResultTotals");

    for (int code = 0; code < xe::TESTSTATUSCODE_LAST; code++)
    {
        writer << Writer::Attribute(xe::getTestStatusCodeName((xe::TestStatusCode)code),
                                    de::toString(totals.countByCode[code]).c_str());
        totalCases += totals.countByCode[code];
    }

    writer << Writer::Attribute("All", de::toString(totalCases).c_str()) << Writer::EndElement;
}

static void batchResultToSingleXmlFile(const char *batchResultFilename, const char *dstFileName)
{
    std::ofstream out(dstFileName, std::ios_base::binary);
    xe::xml::Writer writer(out);
    BatchResultTotals totals;
    ResultToSingleXmlLogHandler handler(writer, totals);
    xe::TestLogParser parser(&handler);

    XE_CHECK(out.good());

    out << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
        << "<?xml-stylesheet href=\"" << TESTCASE_STYLESHEET << "\" type=\"text/xsl\"?>\n";

    writer << xe::xml::Writer::BeginElement("BatchResult")
           << xe::xml::Writer::Attribute("FileName", de::FilePath(batchResultFilename).getBaseName());

    // Parse and write individual cases
    parseBatchResult(parser, batchResultFilename);

    // Write ResultTotals
    writeTotals(writer, totals);

    writer << xe::xml::Writer::EndElement;
    out << "\n";
}

// Export to separate files

class ResultToXmlFilesLogHandler : public xe::TestLogHandler
{
public:
    ResultToXmlFilesLogHandler(vector<xe::TestCaseResultHeader> &resultHeaders, const char *dstPath)
        : m_resultHeaders(resultHeaders)
        , m_dstPath(dstPath)
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
        xe::TestCaseResult result;

        xe::parseTestCaseResultFromData(&m_resultParser, &result, *resultData.get());

        // Write result.
        {
            de::FilePath casePath = de::FilePath::join(m_dstPath, (result.casePath + ".xml").c_str());
            std::ofstream out(casePath.getPath(), std::ofstream::binary | std::ofstream::trunc);
            xe::xml::Writer xmlWriter(out);

            if (!out.good())
                throw xe::Error(string("Failed to open ") + casePath.getPath());

            out << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
                << "<?xml-stylesheet href=\"" << TESTCASE_STYLESHEET << "\" type=\"text/xsl\"?>\n";
            xe::writeTestResult(result, xmlWriter);
            out << "\n";
        }

        m_resultHeaders.push_back(xe::TestCaseResultHeader(result));
    }

private:
    vector<xe::TestCaseResultHeader> &m_resultHeaders;
    std::string m_dstPath;
    xe::TestResultParser m_resultParser;
};

typedef std::map<const xe::TestCase *, const xe::TestCaseResultHeader *> ShortTestResultMap;

static void writeTestCaseListNode(const xe::TestNode *testNode, const ShortTestResultMap &resultMap,
                                  xe::xml::Writer &dst)
{
    using xe::xml::Writer;

    bool isGroup = testNode->getNodeType() == xe::TESTNODETYPE_GROUP;
    string fullPath;
    testNode->getFullPath(fullPath);

    if (isGroup)
    {
        const xe::TestGroup *group = static_cast<const xe::TestGroup *>(testNode);

        dst << Writer::BeginElement("TestGroup") << Writer::Attribute("Name", testNode->getName());

        for (int childNdx = 0; childNdx < group->getNumChildren(); childNdx++)
            writeTestCaseListNode(group->getChild(childNdx), resultMap, dst);

        dst << Writer::EndElement;
    }
    else
    {
        DE_ASSERT(testNode->getNodeType() == xe::TESTNODETYPE_TEST_CASE);

        const xe::TestCase *testCase                 = static_cast<const xe::TestCase *>(testNode);
        ShortTestResultMap::const_iterator resultPos = resultMap.find(testCase);
        const xe::TestCaseResultHeader *result       = resultPos != resultMap.end() ? resultPos->second : nullptr;

        DE_ASSERT(result);

        dst << Writer::BeginElement("TestCase") << Writer::Attribute("Name", testNode->getName())
            << Writer::Attribute("Type", xe::getTestCaseTypeName(result->caseType))
            << Writer::Attribute("StatusCode", xe::getTestStatusCodeName(result->statusCode))
            << Writer::Attribute("StatusDetails", result->statusDetails.c_str()) << Writer::EndElement;
    }
}

static void writeTestCaseList(const xe::TestRoot &root, const ShortTestResultMap &resultMap, xe::xml::Writer &dst)
{
    using xe::xml::Writer;

    dst << Writer::BeginElement("TestRoot");

    for (int childNdx = 0; childNdx < root.getNumChildren(); childNdx++)
        writeTestCaseListNode(root.getChild(childNdx), resultMap, dst);

    dst << Writer::EndElement;
}

static void batchResultToSeparateXmlFiles(const char *batchResultFilename, const char *dstPath)
{
    xe::TestRoot testRoot;
    vector<xe::TestCaseResultHeader> shortResults;
    ShortTestResultMap resultMap;

    // Initialize destination directory.
    if (!de::FilePath(dstPath).exists())
        de::createDirectoryAndParents(dstPath);
    else
        XE_CHECK_MSG(de::FilePath(dstPath).getType() == de::FilePath::TYPE_DIRECTORY, "Destination is not directory");

    // Parse batch result and write out test cases.
    {
        ResultToXmlFilesLogHandler handler(shortResults, dstPath);
        xe::TestLogParser parser(&handler);

        parseBatchResult(parser, batchResultFilename);
    }

    // Build case hierarchy & short result map.
    {
        xe::TestHierarchyBuilder hierarchyBuilder(&testRoot);

        for (vector<xe::TestCaseResultHeader>::const_iterator result = shortResults.begin();
             result != shortResults.end(); result++)
        {
            xe::TestCase *testCase = hierarchyBuilder.createCase(result->casePath.c_str(), result->caseType);
            resultMap.insert(std::make_pair(testCase, &(*result)));
        }
    }

    // Create caselist.
    {
        de::FilePath indexPath = de::FilePath::join(dstPath, "caselist.xml");
        std::ofstream out(indexPath.getPath(), std::ofstream::binary | std::ofstream::trunc);
        xe::xml::Writer xmlWriter(out);

        XE_CHECK_MSG(out.good(), "Failed to open caselist.xml");

        out << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
            << "<?xml-stylesheet href=\"" << CASELIST_STYLESHEET << "\" type=\"text/xsl\"?>\n";
        writeTestCaseList(testRoot, resultMap, xmlWriter);
        out << "\n";
    }
}

int main(int argc, const char *const *argv)
{
    try
    {
        CommandLine cmdLine;
        if (!parseCommandLine(cmdLine, argc, argv))
            return -1;

        if (cmdLine.outputMode == OUTPUTMODE_SINGLE)
            batchResultToSingleXmlFile(cmdLine.batchResultFile.c_str(), cmdLine.outputPath.c_str());
        else
            batchResultToSeparateXmlFiles(cmdLine.batchResultFile.c_str(), cmdLine.outputPath.c_str());
    }
    catch (const std::exception &e)
    {
        printf("%s\n", e.what());
        return -1;
    }

    return 0;
}
