/*
 * Copyright (C) 2013 University of Szeged. All rights reserved.
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY UNIVERSITY OF SZEGED ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL UNIVERSITY OF SZEGED OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "Options.h"

#include <string.h>

namespace WTR {

Options::Options(double defaultLongTimeout, double defaultShortTimeout)
    : longTimeout(defaultLongTimeout)
    , shortTimeout(defaultShortTimeout)
    , useWaitToDumpWatchdogTimer(true)
    , forceNoTimeout(false)
    , verbose(false)
    , gcBetweenTests(false)
    , shouldDumpPixelsForAllTests(false)
    , printSupportedFeatures(false)
    , forceComplexText(false)
    , shouldUseAcceleratedDrawing(false)
    , shouldUseRemoteLayerTree(false)
    , defaultLongTimeout(defaultLongTimeout)
    , defaultShortTimeout(defaultShortTimeout)
{
}

bool handleOptionTimeout(Options& options, const char*, const char* argument)
{
    options.longTimeout = atoi(argument);
    // Scale up the short timeout to match.
    options.shortTimeout = options.defaultShortTimeout * options.longTimeout / options.defaultLongTimeout;
    return true;
}

bool handleOptionNoTimeout(Options& options, const char*, const char*)
{
    options.useWaitToDumpWatchdogTimer = false;
    return true;
}

bool handleOptionNoTimeoutAtAll(Options& options, const char*, const char*)
{
    options.useWaitToDumpWatchdogTimer = false;
    options.forceNoTimeout = true;
    return true;
}

bool handleOptionVerbose(Options& options, const char*, const char*)
{
    options.verbose = true;
    return true;
}

bool handleOptionGcBetweenTests(Options& options, const char*, const char*)
{
    options.gcBetweenTests = true;
    return true;
}

bool handleOptionPixelTests(Options& options, const char*, const char*)
{
    options.shouldDumpPixelsForAllTests = true;
    return true;
}

bool handleOptionPrintSupportedFeatures(Options& options, const char*, const char*)
{
    options.printSupportedFeatures = true;
    return true;
}

bool handleOptionComplexText(Options& options, const char*, const char*)
{
    options.forceComplexText = true;
    return true;
}

bool handleOptionAcceleratedDrawing(Options& options, const char*, const char*)
{
    options.shouldUseAcceleratedDrawing = true;
    return true;
}

bool handleOptionRemoteLayerTree(Options& options, const char*, const char*)
{
    options.shouldUseRemoteLayerTree = true;
    return true;
}

bool handleOptionUnmatched(Options& options, const char* option, const char*)
{
    if (option[0] && option[1] && option[0] == '-' && option[1] == '-')
        return true;
    options.paths.push_back(option);
    return true;
}

OptionsHandler::OptionsHandler(Options& o)
    : options(o)
{
    optionList.append(Option("--timeout", "Sets long timeout to <param> and scales short timeout.", handleOptionTimeout, true));
    optionList.append(Option("--no-timeout", "Disables timeout.", handleOptionNoTimeout));
    optionList.append(Option("--no-timeout-at-all", "Disables all timeouts.", handleOptionNoTimeoutAtAll));
    optionList.append(Option("--verbose", "Turns on messages.", handleOptionVerbose));
    optionList.append(Option("--gc-between-tests", "Garbage collection between tests.", handleOptionGcBetweenTests));
    optionList.append(Option("--pixel-tests", "Check pixels.", handleOptionPixelTests));
    optionList.append(Option("-p", "Check pixels.", handleOptionPixelTests));
    optionList.append(Option("--print-supported-features", "For DumpRenderTree compatibility.", handleOptionPrintSupportedFeatures));
    optionList.append(Option("--complex-text", "Force complex tests.", handleOptionComplexText));
    optionList.append(Option("--accelerated-drawing", "Use accelerated drawing.", handleOptionAcceleratedDrawing));
    optionList.append(Option("--remote-layer-tree", "Use remote layer tree.", handleOptionRemoteLayerTree));
    optionList.append(Option(0, 0, handleOptionUnmatched));
}

const char * OptionsHandler::usage = "Usage: WebKitTestRunner [options] filename [filename2..n]";
const char * OptionsHandler::help = "Displays this help.";

Option::Option(const char* name, const char* description, std::function<bool(Options&, const char*, const char*)> parameterHandler, bool hasArgument)
    : name(name), description(description), parameterHandler(parameterHandler), hasArgument(hasArgument) { };

bool Option::matches(const char* option)
{
    return !name || !strcmp(name, option);
}

bool OptionsHandler::parse(int argc, const char* argv[])
{
    bool status = true;
    for (int argCounter = 1; argCounter < argc; ++argCounter) {
        if (!strcmp(argv[argCounter], "--help") || !strcmp(argv[argCounter], "-h")) {
            printHelp();
            return false;
        }
        const char* currentOption = argv[argCounter];
        for (Option& option : optionList) {
            if (option.matches(currentOption)) {
                if (!option.parameterHandler)
                    status = false;
                else if (option.hasArgument) {
                    const char * currentArgument = argv[++argCounter];
                    if (currentArgument)
                        status &= option.parameterHandler(options, currentOption, currentArgument);
                    else
                        status = false;
                } else
                    status &= option.parameterHandler(options, currentOption, 0);
                break;
            }
        }
    }
    return status;
}

void OptionsHandler::printHelp(FILE* channel)
{
    fputs(usage, channel);
    fputs("\n\n    -h|--help\n\t", channel);
    fputs(help, channel);
    fputs("\n\n", channel);
    for (Option& option : optionList) {
        if (!option.name)
            continue;
        fputs("    ", channel);
        fputs(option.name, channel);
        fputs((option.hasArgument ? " <param>\n\t" : "\n\t"), channel);
        fputs(option.description, channel);
        fputs("\n\n", channel);
    }
}

} // namespace WTR
