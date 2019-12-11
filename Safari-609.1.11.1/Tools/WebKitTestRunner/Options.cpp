/*
 * Copyright (C) 2013 University of Szeged. All rights reserved.
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
 * Copyright (C) 2015-2018 Apple Inc. All rights reserved.
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

static bool handleOptionNoTimeout(Options& options, const char*, const char*)
{
    options.useWaitToDumpWatchdogTimer = false;
    options.forceNoTimeout = true;
    return true;
}

static bool handleOptionVerbose(Options& options, const char*, const char*)
{
    options.verbose = true;
    return true;
}

static bool handleOptionGcBetweenTests(Options& options, const char*, const char*)
{
    options.gcBetweenTests = true;
    return true;
}

static bool handleOptionPixelTests(Options& options, const char*, const char*)
{
    options.shouldDumpPixelsForAllTests = true;
    return true;
}

static bool handleOptionComplexText(Options& options, const char*, const char*)
{
    options.forceComplexText = true;
    return true;
}

static bool handleOptionAcceleratedDrawing(Options& options, const char*, const char*)
{
    options.shouldUseAcceleratedDrawing = true;
    return true;
}

static bool handleOptionRemoteLayerTree(Options& options, const char*, const char*)
{
    options.shouldUseRemoteLayerTree = true;
    return true;
}

static bool handleOptionShowWebView(Options& options, const char*, const char*)
{
    options.shouldShowWebView = true;
    return true;
}

static bool handleOptionShowTouches(Options& options, const char*, const char*)
{
    options.shouldShowTouches = true;
    return true;
}

static bool handleOptionCheckForWorldLeaks(Options& options, const char*, const char*)
{
    options.checkForWorldLeaks = true;
    return true;
}

static bool handleOptionAllowAnyHTTPSCertificateForAllowedHosts(Options& options, const char*, const char*)
{
    options.allowAnyHTTPSCertificateForAllowedHosts = true;
    return true;
}

static bool handleOptionAllowedHost(Options& options, const char*, const char* host)
{
    options.allowedHosts.insert(host);
    return true;
}

static bool handleOptionUnmatched(Options& options, const char* option, const char*)
{
    if (option[0] && option[1] && option[0] == '-' && option[1] == '-')
        return true;
    options.paths.push_back(option);
    return true;
}

OptionsHandler::OptionsHandler(Options& o)
    : options(o)
{
    optionList.append(Option("--no-timeout", "Disables all timeouts.", handleOptionNoTimeout));
    optionList.append(Option("--verbose", "Turns on messages.", handleOptionVerbose));
    optionList.append(Option("--gc-between-tests", "Garbage collection between tests.", handleOptionGcBetweenTests));
    optionList.append(Option("--pixel-tests", "Check pixels.", handleOptionPixelTests));
    optionList.append(Option("-p", "Check pixels.", handleOptionPixelTests));
    optionList.append(Option("--complex-text", "Force complex tests.", handleOptionComplexText));
    optionList.append(Option("--accelerated-drawing", "Use accelerated drawing.", handleOptionAcceleratedDrawing));
    optionList.append(Option("--remote-layer-tree", "Use remote layer tree.", handleOptionRemoteLayerTree));
    optionList.append(Option("--allowed-host", "Allows access to the specified host from tests.", handleOptionAllowedHost, true));
    optionList.append(Option("--allow-any-certificate-for-allowed-hosts", "Allows any HTTPS certificate for an allowed host.", handleOptionAllowAnyHTTPSCertificateForAllowedHosts));
    optionList.append(Option("--show-webview", "Show the WebView during test runs (for debugging)", handleOptionShowWebView));
    optionList.append(Option("--show-touches", "Show the touches during test runs (for debugging)", handleOptionShowTouches));
    optionList.append(Option("--world-leaks", "Check for leaks of world objects (currently, documents)", handleOptionCheckForWorldLeaks));

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
