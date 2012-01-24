/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "XMLHttpRequestException.h"

namespace WebCore {

// FIXME: This should be an array of structs to pair the names and descriptions.
static const char* const exceptionNames[] = {
    "NETWORK_ERR",
    "ABORT_ERR"
};

static const char* const exceptionDescriptions[] = {
    "A network error occurred in synchronous requests.",
    "The user aborted a request in synchronous requests."
};

COMPILE_ASSERT(WTF_ARRAY_LENGTH(exceptionNames) == WTF_ARRAY_LENGTH(exceptionDescriptions), XMLHttpRequestExceptionTablesMustMatch);

bool XMLHttpRequestException::initializeDescription(ExceptionCode ec, ExceptionCodeDescription* description)
{
    if (ec < XMLHttpRequestExceptionOffset || ec > XMLHttpRequestExceptionMax)
        return false;

    description->typeName = "XMLHttpRequest";
    description->code = ec - XMLHttpRequestExceptionOffset;
    description->type = XMLHttpRequestExceptionType;

    size_t tableSize = WTF_ARRAY_LENGTH(exceptionNames);
    size_t tableIndex = ec - NETWORK_ERR;

    description->name = tableIndex < tableSize ? exceptionNames[tableIndex] : 0;
    description->description = tableIndex < tableSize ? exceptionDescriptions[tableIndex] : 0;

    return true;
}

} // namespace WebCore
