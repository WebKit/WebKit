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
 * \brief Extract sample lists from logs.
 *//*--------------------------------------------------------------------*/

#include "xeTestLogParser.hpp"
#include "xeTestResultParser.hpp"
#include "deFilePath.hpp"
#include "deString.h"
#include "deStringUtil.hpp"

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

void writeSampleList(const char *casePath, int listNdx, const xe::ri::SampleList &sampleList)
{
    const string filename = string(casePath) + "." + de::toString(listNdx) + ".csv";
    std::ofstream out(filename.c_str(), std::ios_base::binary);

    if (!out.good())
        throw std::runtime_error("Failed to open " + filename);

    // Header
    for (int ndx = 0; ndx < sampleList.sampleInfo.valueInfos.getNumItems(); ndx++)
    {
        if (ndx != 0)
            out << ",";
        out << static_cast<const xe::ri::ValueInfo &>(sampleList.sampleInfo.valueInfos.getItem(ndx)).name;
    }
    out << "\n";

    // Samples
    for (int sampleNdx = 0; sampleNdx < sampleList.samples.getNumItems(); sampleNdx++)
    {
        const xe::ri::Sample &sample = static_cast<const xe::ri::Sample &>(sampleList.samples.getItem(sampleNdx));

        for (int valNdx = 0; valNdx < sample.values.getNumItems(); valNdx++)
        {
            const xe::ri::SampleValue &value = static_cast<const xe::ri::SampleValue &>(sample.values.getItem(valNdx));

            if (valNdx != 0)
                out << ",";

            out << value.value;
        }
        out << "\n";
    }
}

void extractSampleLists(const char *casePath, int *listNdx, const xe::ri::List &items)
{
    for (int itemNdx = 0; itemNdx < items.getNumItems(); itemNdx++)
    {
        const xe::ri::Item &child = items.getItem(itemNdx);

        if (child.getType() == xe::ri::TYPE_SECTION)
            extractSampleLists(casePath, listNdx, static_cast<const xe::ri::Section &>(child).items);
        else if (child.getType() == xe::ri::TYPE_SAMPLELIST)
        {
            writeSampleList(casePath, *listNdx, static_cast<const xe::ri::SampleList &>(child));
            *listNdx += 1;
        }
    }
}

void extractSampleLists(const xe::TestCaseResult &result)
{
    int listNdx = 0;
    extractSampleLists(result.casePath.c_str(), &listNdx, result.resultItems);
}

class SampleListParser : public xe::TestLogHandler
{
public:
    SampleListParser(void)
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
        xe::TestCaseResult result;
        xe::parseTestCaseResultFromData(&m_testResultParser, &result, *caseData.get());
        extractSampleLists(result);
    }

private:
    xe::TestResultParser m_testResultParser;
};

static void processLogFile(const char *filename)
{
    std::ifstream in(filename, std::ifstream::binary | std::ifstream::in);
    SampleListParser resultHandler;
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

int main(int argc, const char *const *argv)
{
    if (argc != 2)
    {
        printf("%s: [filename]\n", de::FilePath(argv[0]).getBaseName().c_str());
        return -1;
    }

    try
    {
        processLogFile(argv[1]);
    }
    catch (const std::exception &e)
    {
        printf("FATAL ERROR: %s\n", e.what());
        return -1;
    }

    return 0;
}
