/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "AST/ASTStringDumper.h"
#include "WGSL.h"
#include "WGSLShaderModule.h"
#include <wtf/DataLog.h>
#include <wtf/FileSystem.h>
#include <wtf/WTFProcess.h>

static NO_RETURN void printUsageStatement(bool help = false)
{
    fprintf(stderr, "Usage: wgsl [options] <file> <entrypoint>\n");
    fprintf(stderr, "  -h|--help  Prints this help message\n");
    fprintf(stderr, "  --dump-ast-after-checking  Dumps the AST after parsing and checking\n");
    fprintf(stderr, "  --dump-ast-at-end  Dumps the AST after generating code\n");
    fprintf(stderr, "  --dump-generated-code  Dumps the generated Metal code\n");
    fprintf(stderr, "\n");

    exitProcess(help ? EXIT_SUCCESS : EXIT_FAILURE);
}

struct CommandLine {
public:
    CommandLine(int argc, char** argv)
    {
        parseArguments(argc, argv);
    }

    const char* file() const { return m_file; }
    const char* entrypoint() const { return m_entrypoint; }
    bool dumpASTAfterCheck() const { return m_dumpASTAfterCheck; }
    bool dumpASTAtEnd() const { return m_dumpASTAtEnd; }
    bool dumpGeneratedCode() const { return m_dumpGeneratedCode; }

private:
    void parseArguments(int, char**);

    const char* m_file { nullptr };
    const char* m_entrypoint { nullptr };
    bool m_dumpASTAfterCheck { false };
    bool m_dumpASTAtEnd { false };
    bool m_dumpGeneratedCode { false };
};

void CommandLine::parseArguments(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        if (!strcmp(arg, "-h") || !strcmp(arg, "--help"))
            printUsageStatement(true);

        if (!strcmp(arg, "--dump-ast-after-checking")) {
            m_dumpASTAfterCheck = true;
            continue;
        }

        if (!strcmp(arg, "--dump-ast-at-end")) {
            m_dumpASTAtEnd = true;
            continue;
        }

        if (!strcmp(arg, "--dump-generated-code")) {
            m_dumpGeneratedCode = true;
            continue;
        }

        if (!m_file)
            m_file = arg;
        else if (!m_entrypoint)
            m_entrypoint = arg;
        else
            printUsageStatement(false);
    }

    if (!m_file || !m_entrypoint)
        printUsageStatement(false);
}

static int runWGSL(const CommandLine& options)
{
    WGSL::Configuration configuration;


    String fileName = String::fromLatin1(options.file());
    auto handle = FileSystem::openFile(fileName, FileSystem::FileOpenMode::Read);
    if (!FileSystem::isHandleValid(handle)) {
        FileSystem::closeFile(handle);
        dataLogLn("Failed to open ", fileName);
        return EXIT_FAILURE;
    }

    auto readResult = FileSystem::readEntireFile(handle);
    FileSystem::closeFile(handle);
    auto source = emptyString();
    if (readResult.has_value())
        source = String::fromUTF8WithLatin1Fallback(readResult->data(), readResult->size());
    auto checkResult = WGSL::staticCheck(source, std::nullopt, configuration);
    if (auto* failedCheck = std::get_if<WGSL::FailedCheck>(&checkResult)) {
        for (const auto& error : failedCheck->errors)
            dataLogLn(error);
        return EXIT_FAILURE;
    }

    auto& shaderModule = std::get<WGSL::SuccessfulCheck>(checkResult).ast;
    if (options.dumpASTAfterCheck())
        WGSL::AST::dumpAST(shaderModule);

    String entrypointName = String::fromLatin1(options.entrypoint());
    auto prepareResult = WGSL::prepare(shaderModule, entrypointName, std::nullopt);

    if (entrypointName != "_"_s && !prepareResult.entryPoints.contains(entrypointName)) {
        dataLogLn("WGSL source does not contain entrypoint named '", entrypointName, "'");
        return EXIT_FAILURE;
    }

    HashMap<String, WGSL::ConstantValue> constantValues;
    auto msl = WGSL::generate(prepareResult.callGraph, constantValues);

    if (options.dumpASTAtEnd())
        WGSL::AST::dumpAST(shaderModule);

    if (options.dumpGeneratedCode())
        printf("%s", msl.utf8().data());

    return EXIT_SUCCESS;
}

int main(int argc, char** argv)
{
    WTF::initializeMainThread();

    CommandLine commandLine(argc, argv);
    return runWGSL(commandLine);
}
