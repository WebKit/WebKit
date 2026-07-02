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
 * \brief Extract shader programs from log.
 *//*--------------------------------------------------------------------*/

#include "xeTestLogParser.hpp"
#include "xeTestResultParser.hpp"
#include "deFilePath.hpp"
#include "deStringUtil.hpp"
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
    CommandLine(void)
    {
    }

    string filename;
    string dstPath;
};

static const char *getShaderTypeSuffix(const xe::ri::Shader::ShaderType shaderType)
{
    switch (shaderType)
    {
    case xe::ri::Shader::SHADERTYPE_VERTEX:
        return "vert";
    case xe::ri::Shader::SHADERTYPE_FRAGMENT:
        return "frag";
    case xe::ri::Shader::SHADERTYPE_GEOMETRY:
        return "geom";
    case xe::ri::Shader::SHADERTYPE_TESS_CONTROL:
        return "tesc";
    case xe::ri::Shader::SHADERTYPE_TESS_EVALUATION:
        return "tese";
    case xe::ri::Shader::SHADERTYPE_COMPUTE:
        return "comp";
    case xe::ri::Shader::SHADERTYPE_RAYGEN:
        return "rgen";
    case xe::ri::Shader::SHADERTYPE_ANY_HIT:
        return "ahit";
    case xe::ri::Shader::SHADERTYPE_CLOSEST_HIT:
        return "chit";
    case xe::ri::Shader::SHADERTYPE_MISS:
        return "miss";
    case xe::ri::Shader::SHADERTYPE_INTERSECTION:
        return "sect";
    case xe::ri::Shader::SHADERTYPE_CALLABLE:
        return "call";
    case xe::ri::Shader::SHADERTYPE_TASK:
        return "task";
    case xe::ri::Shader::SHADERTYPE_MESH:
        return "mesh";

    default:
        throw xe::Error("Invalid shader type");
    }
}

static void writeShaderProgram(const CommandLine &cmdLine, const std::string &casePath,
                               const xe::ri::ShaderProgram &shaderProgram, int programNdx)
{
    const string basePath =
        string(de::FilePath::join(cmdLine.dstPath, casePath).getPath()) + "." + de::toString(programNdx);

    for (int shaderNdx = 0; shaderNdx < shaderProgram.shaders.getNumItems(); shaderNdx++)
    {
        const xe::ri::Shader &shader = dynamic_cast<const xe::ri::Shader &>(shaderProgram.shaders.getItem(shaderNdx));
        const string shaderPath      = basePath + "." + getShaderTypeSuffix(shader.shaderType);

        if (de::FilePath(shaderPath).exists())
            throw xe::Error("File '" + shaderPath + "' exists already");

        {
            std::ofstream out(shaderPath.c_str(), std::ifstream::binary | std::ifstream::out);

            if (!out.good())
                throw xe::Error("Failed to open '" + shaderPath + "'");

            out.write(shader.source.source.c_str(), shader.source.source.size());
        }
    }
}

struct StackEntry
{
    const xe::ri::List *list;
    int curNdx;

    explicit StackEntry(const xe::ri::List *list_) : list(list_), curNdx(0)
    {
    }
};

static void extractShaderPrograms(const CommandLine &cmdLine, const std::string &casePath,
                                  const xe::TestCaseResult &result)
{
    vector<StackEntry> itemListStack;
    int programNdx = 0;

    itemListStack.push_back(StackEntry(&result.resultItems));

    while (!itemListStack.empty())
    {
        StackEntry &curEntry = itemListStack.back();

        if (curEntry.curNdx < curEntry.list->getNumItems())
        {
            const xe::ri::Item &curItem = curEntry.list->getItem(curEntry.curNdx);
            curEntry.curNdx += 1;

            if (curItem.getType() == xe::ri::TYPE_SHADERPROGRAM)
            {
                writeShaderProgram(cmdLine, casePath, static_cast<const xe::ri::ShaderProgram &>(curItem), programNdx);
                programNdx += 1;
            }
            else if (curItem.getType() == xe::ri::TYPE_SECTION)
                itemListStack.push_back(StackEntry(&static_cast<const xe::ri::Section &>(curItem).items));
        }
        else
            itemListStack.pop_back();
    }

    if (programNdx == 0)
        std::cout << "WARNING: no shader programs found in '" << casePath << "'\n";
}

class ShaderProgramExtractHandler : public xe::TestLogHandler
{
public:
    ShaderProgramExtractHandler(const CommandLine &cmdLine) : m_cmdLine(cmdLine)
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
        if (caseData->getDataSize() > 0)
        {
            xe::TestCaseResult fullResult;
            xe::TestResultParser::ParseResult parseResult;

            m_testResultParser.init(&fullResult);
            parseResult = m_testResultParser.parse(caseData->getData(), caseData->getDataSize());
            DE_UNREF(parseResult);

            extractShaderPrograms(m_cmdLine, caseData->getTestCasePath(), fullResult);
        }
    }

private:
    const CommandLine &m_cmdLine;
    xe::TestResultParser m_testResultParser;
};

static void extractShaderProgramsFromLogFile(const CommandLine &cmdLine)
{
    std::ifstream in(cmdLine.filename.c_str(), std::ifstream::binary | std::ifstream::in);
    ShaderProgramExtractHandler resultHandler(cmdLine);
    xe::TestLogParser parser(&resultHandler);
    uint8_t buf[1024];
    int numRead = 0;

    if (!in.good())
        throw std::runtime_error(string("Failed to open '") + cmdLine.filename + "'");

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

static void printHelp(const char *binName)
{
    printf("%s: [filename] [dst path (optional)]\n", binName);
}

static bool parseCommandLine(CommandLine &cmdLine, int argc, const char *const *argv)
{
    for (int argNdx = 1; argNdx < argc; argNdx++)
    {
        const char *arg = argv[argNdx];

        if (!deStringBeginsWith(arg, "--"))
        {
            if (cmdLine.filename.empty())
                cmdLine.filename = arg;
            else if (cmdLine.dstPath.empty())
                cmdLine.dstPath = arg;
            else
                return false;
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

        extractShaderProgramsFromLogFile(cmdLine);
    }
    catch (const std::exception &e)
    {
        printf("FATAL ERROR: %s\n", e.what());
        return -1;
    }

    return 0;
}
