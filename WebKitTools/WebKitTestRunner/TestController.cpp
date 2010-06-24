/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "TestController.h"

#include "TestInvocation.h"
#include <getopt.h>

namespace WTR {

TestController& TestController::shared()
{
    static TestController& shared = *new TestController;
    return shared;
}

TestController::TestController()
    : m_dumpTree(false)
    , m_dumpPixels(false)
    , m_threaded(false)
    , m_forceComplexText(false)    
    , m_verbose(false)
    , m_printSeparators(false)
    , m_usingServerMode(false)
{
}

void TestController::initialize(int argc, const char *argv[])
{
    int dumpTree = 0;
    int dumpPixels = 0;
    int threaded = 0;
    int forceComplexText = 0;
    int useHTML5Parser = 0;
    int verbose = 0;

    struct option options[] = {
        {"notree", no_argument, &dumpTree, false},
        {"pixel-tests", no_argument, &dumpPixels, true},
        {"tree", no_argument, &dumpTree, true},
        {"threaded", no_argument, &threaded, true},
        {"complex-text", no_argument, &forceComplexText, true},
        {"legacy-parser", no_argument, &useHTML5Parser, false},
        {"verbose", no_argument, &verbose, true},
        {0, 0, 0, 0}
    };
    
    int option;
    while ((option = getopt_long(argc, (char * const *)argv, "", options, 0)) != -1) {
        switch (option) {
            case '?':   // unknown or ambiguous option
            case ':':   // missing argument
                exit(1);
                break;
        }
    }

    m_dumpTree = dumpTree;
    m_dumpPixels = dumpPixels;
    m_threaded = threaded;
    m_forceComplexText = forceComplexText;
    m_verbose = verbose;

    m_usingServerMode = (argc == optind + 1 && strcmp(argv[optind], "-") == 0);
    if (m_usingServerMode)
        m_printSeparators = true;
    else {
        m_printSeparators = (optind < argc - 1 || (m_dumpPixels && m_dumpTree));
        for (int i = optind; i != argc; ++i)
            m_paths.push_back(std::string(argv[i]));
    }

    initializeInjectedBundlePath();
}

void TestController::runTest(const char* test)
{
    TestInvocation invocation(test);
    invocation.invoke();
}

void TestController::runTestingServerLoop()
{
    char filenameBuffer[2048];
    while (fgets(filenameBuffer, sizeof(filenameBuffer), stdin)) {
        char *newLineCharacter = strchr(filenameBuffer, '\n');
        if (newLineCharacter)
            *newLineCharacter = '\0';

        if (strlen(filenameBuffer) == 0)
            continue;

        runTest(filenameBuffer);
    }
}

bool TestController::run()
{
    if (m_usingServerMode)
        runTestingServerLoop();
    else {
        for (size_t i = 0; i < m_paths.size(); ++i)
            runTest(m_paths[i].c_str());
    }

    return true;
}

} // namespace WTR

