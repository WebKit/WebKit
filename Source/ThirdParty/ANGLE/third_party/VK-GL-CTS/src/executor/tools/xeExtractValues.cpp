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
 * \brief Extract values by name from logs.
 *//*--------------------------------------------------------------------*/

#include "xeTestLogParser.hpp"
#include "xeTestResultParser.hpp"
#include "deFilePath.hpp"
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

struct CommandLine
{
    CommandLine(void) : statusCode(false)
    {
    }

    string filename;
    vector<string> tagNames;
    bool statusCode;
};

typedef xe::ri::NumericValue Value;

struct CaseValues
{
    string casePath;
    xe::TestCaseType caseType;
    xe::TestStatusCode statusCode;
    string statusDetails;

    vector<Value> values;
};

class BatchResultValues
{
public:
    BatchResultValues(const vector<string> &tagNames) : m_tagNames(tagNames)
    {
    }

    ~BatchResultValues(void)
    {
        for (vector<CaseValues *>::iterator i = m_caseValues.begin(); i != m_caseValues.end(); ++i)
            delete *i;
    }

    void add(const CaseValues &result)
    {
        CaseValues *copy = new CaseValues(result);
        try
        {
            m_caseValues.push_back(copy);
        }
        catch (...)
        {
            delete copy;
            throw;
        }
    }

    const vector<string> &getTagNames(void) const
    {
        return m_tagNames;
    }

    size_t size(void) const
    {
        return m_caseValues.size();
    }
    const CaseValues &operator[](size_t ndx) const
    {
        return *m_caseValues[ndx];
    }

private:
    vector<string> m_tagNames;
    vector<CaseValues *> m_caseValues;
};

static Value findValueByTag(const xe::ri::List &items, const string &tagName)
{
    for (int ndx = 0; ndx < items.getNumItems(); ndx++)
    {
        const xe::ri::Item &item = items.getItem(ndx);

        if (item.getType() == xe::ri::TYPE_SECTION)
        {
            const Value value = findValueByTag(static_cast<const xe::ri::Section &>(item).items, tagName);
            if (value.getType() != Value::NUMVALTYPE_EMPTY)
                return value;
        }
        else if (item.getType() == xe::ri::TYPE_NUMBER)
        {
            const xe::ri::Number &value = static_cast<const xe::ri::Number &>(item);
            return value.value;
        }
    }

    return Value();
}

class TagParser : public xe::TestLogHandler
{
public:
    TagParser(BatchResultValues &result) : m_result(result)
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
        const vector<string> &tagNames = m_result.getTagNames();
        CaseValues tagResult;

        tagResult.casePath      = caseData->getTestCasePath();
        tagResult.caseType      = xe::TESTCASETYPE_SELF_VALIDATE;
        tagResult.statusCode    = caseData->getStatusCode();
        tagResult.statusDetails = caseData->getStatusDetails();
        tagResult.values.resize(tagNames.size());

        if (caseData->getDataSize() > 0 && caseData->getStatusCode() == xe::TESTSTATUSCODE_LAST)
        {
            xe::TestCaseResult fullResult;
            xe::TestResultParser::ParseResult parseResult;

            m_testResultParser.init(&fullResult);
            parseResult = m_testResultParser.parse(caseData->getData(), caseData->getDataSize());

            if ((parseResult != xe::TestResultParser::PARSERESULT_ERROR &&
                 fullResult.statusCode != xe::TESTSTATUSCODE_LAST) ||
                (tagResult.statusCode == xe::TESTSTATUSCODE_LAST && fullResult.statusCode != xe::TESTSTATUSCODE_LAST))
            {
                tagResult.statusCode    = fullResult.statusCode;
                tagResult.statusDetails = fullResult.statusDetails;
            }
            else if (tagResult.statusCode == xe::TESTSTATUSCODE_LAST)
            {
                DE_ASSERT(parseResult == xe::TestResultParser::PARSERESULT_ERROR);
                tagResult.statusCode    = xe::TESTSTATUSCODE_INTERNAL_ERROR;
                tagResult.statusDetails = "Test case result parsing failed";
            }

            if (parseResult != xe::TestResultParser::PARSERESULT_ERROR)
            {
                for (int valNdx = 0; valNdx < (int)tagNames.size(); valNdx++)
                    tagResult.values[valNdx] = findValueByTag(fullResult.resultItems, tagNames[valNdx]);
            }
        }

        m_result.add(tagResult);
    }

private:
    BatchResultValues &m_result;
    xe::TestResultParser m_testResultParser;
};

static void readLogFile(BatchResultValues &batchResult, const char *filename)
{
    std::ifstream in(filename, std::ifstream::binary | std::ifstream::in);
    TagParser resultHandler(batchResult);
    xe::TestLogParser parser(&resultHandler);
    uint8_t buf[1024];
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

static void printTaggedValues(const CommandLine &cmdLine, std::ostream &dst)
{
    BatchResultValues values(cmdLine.tagNames);

    readLogFile(values, cmdLine.filename.c_str());

    // Header
    {
        dst << "CasePath";
        if (cmdLine.statusCode)
            dst << ",StatusCode";

        for (vector<string>::const_iterator tagName = values.getTagNames().begin();
             tagName != values.getTagNames().end(); ++tagName)
            dst << "," << *tagName;

        dst << "\n";
    }

    for (int resultNdx = 0; resultNdx < (int)values.size(); resultNdx++)
    {
        const CaseValues &result = values[resultNdx];

        dst << result.casePath;
        if (cmdLine.statusCode)
            dst << "," << xe::getTestStatusCodeName(result.statusCode);

        for (vector<Value>::const_iterator value = result.values.begin(); value != result.values.end(); ++value)
            dst << "," << *value;

        dst << "\n";
    }
}

static void printHelp(const char *binName)
{
    printf("%s: [filename] [name 1] [[name 2]...]\n", binName);
    printf(" --statuscode     Include status code as first entry.\n");
}

static bool parseCommandLine(CommandLine &cmdLine, int argc, const char *const *argv)
{
    for (int argNdx = 1; argNdx < argc; argNdx++)
    {
        const char *arg = argv[argNdx];

        if (strcmp(arg, "--statuscode") == 0)
            cmdLine.statusCode = true;
        else if (!deStringBeginsWith(arg, "--"))
        {
            if (cmdLine.filename.empty())
                cmdLine.filename = arg;
            else
                cmdLine.tagNames.push_back(arg);
        }
        else
            return false;
    }

    if (cmdLine.filename.empty())
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

        printTaggedValues(cmdLine, std::cout);
    }
    catch (const std::exception &e)
    {
        printf("FATAL ERROR: %s\n", e.what());
        return -1;
    }

    return 0;
}
