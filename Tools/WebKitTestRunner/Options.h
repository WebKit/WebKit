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

#ifndef Options_h
#define Options_h

#include <functional>
#include <stdio.h>
#include <string>
#include <vector>
#include <wtf/Vector.h>

namespace WTR {

struct Options {
    Options(double, double);
    double longTimeout;
    double shortTimeout;
    bool useWaitToDumpWatchdogTimer;
    bool forceNoTimeout;
    bool verbose;
    bool gcBetweenTests;
    bool shouldDumpPixelsForAllTests;
    bool printSupportedFeatures;
    bool forceComplexText;
    bool shouldUseAcceleratedDrawing;
    bool shouldUseRemoteLayerTree;
    std::vector<std::string> paths;
    double defaultLongTimeout;
    double defaultShortTimeout;
};

class Option {
public:
    Option(const char* name, const char* description, std::function<bool(Options&, const char*, const char*)> parameterHandler, bool hasArgument = false);
    bool matches(const char*);
    const char* name;
    const char* description;
    std::function<bool(Options&, const char*, const char*)> parameterHandler;
    bool hasArgument;
};

class OptionsHandler {
public:
    explicit OptionsHandler(Options&);
    bool parse(int argc, const char* argv[]);
    void printHelp(FILE* channel = stderr);
private:
    Vector<Option> optionList;
    Options& options;
    static const char* usage;
    static const char* help;
};

} // namespace WTR

#endif // Options_h
