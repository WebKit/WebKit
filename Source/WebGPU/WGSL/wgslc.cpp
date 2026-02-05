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
#include <cstdlib>
#include <wtf/DataLog.h>
#include <wtf/FileSystem.h>
#include <wtf/WTFProcess.h>

[[noreturn]] static void printUsageStatement(bool help = false)
{
    fprintf(stderr, "Usage: wgsl [options] <file> [entrypoint]\n");
    fprintf(stderr, "  -h|--help  Prints this help message\n");
    fprintf(stderr, "  --dump-ast-after-checking  Dumps the AST after parsing and checking\n");
    fprintf(stderr, "  --dump-ast-at-end  Dumps the AST after generating code\n");
    fprintf(stderr, "  --dump-generated-code  Dumps the generated Metal code\n");
    fprintf(stderr, "  --apple-gpu-family=N  Sets the value for the Apple GPU family (default: 4)\n");
    fprintf(stderr, "  --enable-shader-validation  Enables Metal shader validation (default: false)\n");
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
    bool shaderValidationEnabled() const { return m_enableShaderValidation; }
    unsigned appleGPUFamily() const { return m_appleGPUFamily; }

private:
    void parseArguments(int, char**);

    const char* m_file { nullptr };
    const char* m_entrypoint { nullptr };
    bool m_dumpASTAfterCheck { false };
    bool m_dumpASTAtEnd { false };
    bool m_dumpGeneratedCode { false };
    bool m_enableShaderValidation { false };
    unsigned m_appleGPUFamily { 4 };
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
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

        if (!strncmp(arg, "--apple-gpu-family=", 19)) {
            const char* argNum = arg + 19;
            int family = atoi(argNum);
            if (family < 4 || family > 9) {
                fprintf(stderr, "Invalid Apple GPU family: %d\n", family);
                exitProcess(EXIT_FAILURE);
            }
            m_appleGPUFamily = family;
            continue;
        }

        if (!strcmp(arg, "--enable-shader-validation")) {
            m_enableShaderValidation = true;
            continue;
        }

#pragma clang diagnostic pop

        if (!m_file)
            m_file = arg;
        else if (!m_entrypoint)
            m_entrypoint = arg;
        else
            printUsageStatement(false);
    }

    if (!m_file)
        printUsageStatement(false);

    if (!m_entrypoint)
        m_entrypoint = "_";
}

static int runWGSL(const CommandLine& options)
{
    WGSL::Configuration configuration;

    String fileName = String::fromLatin1(options.file());
    auto readResult = FileSystem::readEntireFile(fileName);
    if (!readResult) {
        dataLogLn("Failed to open ", fileName);
        return EXIT_FAILURE;
    }

    auto source = emptyString();
    if (readResult.has_value())
        source = String::fromUTF8WithLatin1Fallback(readResult->span());

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
    HashMap<String, WGSL::PipelineLayout*> pipelineLayouts;
    if (entrypointName != "_"_s)
        pipelineLayouts.add(entrypointName, nullptr);
    else {
        for (auto& entryPoint : shaderModule->callGraph().entrypoints())
            pipelineLayouts.add(entryPoint.originalName, nullptr);
    }
    auto prepareResult = WGSL::prepare(shaderModule, pipelineLayouts);

    if (auto* error = std::get_if<WGSL::Error>(&prepareResult)) {
        dataLogLn(*error);
        return EXIT_FAILURE;
    }

    auto& result = std::get<WGSL::PrepareResult>(prepareResult);
    if (entrypointName != "_"_s && !result.entryPoints.contains(entrypointName)) {
        dataLogLn("WGSL source does not contain entrypoint named '", entrypointName, "'");
        return EXIT_FAILURE;
    }

    HashMap<String, WGSL::ConstantValue> userDefinedValues;
    auto generationResult = WGSL::generate(shaderModule, result, userDefinedValues, WGSL::DeviceState {
        .appleGPUFamily = options.appleGPUFamily(),
        .shaderValidationEnabled = options.shaderValidationEnabled(),
    });

    if (auto* error = std::get_if<WGSL::Error>(&generationResult)) {
        dataLogLn(*error);
        return EXIT_FAILURE;
    }

    auto& msl = std::get<String>(generationResult);

    if (options.dumpASTAtEnd())
        WGSL::AST::dumpAST(shaderModule);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
    if (options.dumpGeneratedCode())
        printf("%s", msl.utf8().data());
#pragma clang diagnostic pop

    return EXIT_SUCCESS;
}

int main(int argc, char** argv)
{
    WTF::initializeMainThread();

    CommandLine commandLine(argc, argv);
    return runWGSL(commandLine);
}
