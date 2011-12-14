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

#include "config.h"
#include "CommandLine.h"

namespace WebKit {

bool CommandLine::parse(LPTSTR commandLineString)
{
    m_args.clear();

    // Check if this is an empty command line.
    if (!commandLineString || !commandLineString[0])
        return true;

    int argumentCount;
    LPWSTR* commandLineArgs = ::CommandLineToArgvW(commandLineString, &argumentCount);
    if (!commandLineArgs)
        return false;

    if (argumentCount % 2) {
        ::LocalFree(commandLineArgs);
        return false;
    }

    for (int i = 0; i < argumentCount ; i += 2) {
        LPWSTR key = commandLineArgs[i];

        if (key[0] != '-') {
            ::LocalFree(commandLineArgs);
            return false;
        }

        m_args.set(&key[1], commandLineArgs[i + 1]);
    }

    ::LocalFree(commandLineArgs);
    return true;
}

} // namespace WebKit
