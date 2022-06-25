/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#include "CommandLine.h"
#include <getopt.h>
#include <iostream>

struct option CommandLine::longOptions[] =
{
    {"benchmark", required_argument, 0, 'b'},
    {"detailed-report", no_argument, 0, 'd'},
    {"no-warmup", no_argument, 0, 'n' },
    {"parallel", no_argument, 0, 'p'},
    {"heap", required_argument, 0, 'h'},
    {"runs", required_argument, 0, 'r'},
    {"use-thread-id", no_argument, 0, 't'},
    {0, 0, 0, 0}
};

CommandLine::CommandLine(int argc, char** argv)
    : m_argv(argv)
    , m_detailedReport(false)
    , m_isParallel(false)
    , m_useThreadID(false)
    , m_warmUp(true)
    , m_heapSize(0)
    , m_runs(8)
{
    int optionIndex = 0;
    int ch;
    while ((ch = getopt_long(argc, argv, "b:dnph:r:t", longOptions, &optionIndex)) != -1) {
        switch (ch)
        {
            case 'b':
                m_benchmarkName = optarg;
                break;

            case 'd':
                m_detailedReport = true;
                break;
                
            case 'n':
                m_warmUp = false;
                break;

            case 'p':
                m_isParallel = true;
                break;
                
            case 'h':
                m_heapSize = atoi(optarg) * 1024 * 1024;
                break;

            case 'r':
                m_runs = atoi(optarg);
                break;

            case 't':
                m_useThreadID = true;
                break;
                
            default:
                break;
        }
    }
}

void CommandLine::printUsage()
{
    std::string fullPath(m_argv[0]);
    size_t pos = fullPath.find_last_of("/") + 1;
    std::string program = fullPath.substr(pos);
    std::cout << "Usage: " << program << " --benchmark benchmark_name [--parallel ] [--use-thread-id ] [--detailed-report] [--no-warmup] [--runs count] [--heap MB ]" << std::endl;
}
