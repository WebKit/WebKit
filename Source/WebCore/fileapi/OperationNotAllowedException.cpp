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

#if ENABLE(BLOB)

#include "OperationNotAllowedException.h"

namespace WebCore {

static struct OperationNotAllowedExceptionNameDescription {
    const char* const name;
    const char* const description;
} operationNotAllowedExceptions[] = {
    { "NOT_ALLOWED_ERR", "A read method was called while the object was in the LOADING state due to a previous read call." }
};

bool OperationNotAllowedException::initializeDescription(ExceptionCode ec, ExceptionCodeDescription* description)
{
    if (ec < OperationNotAllowedExceptionOffset || ec > OperationNotAllowedExceptionMax)
        return false;

    description->typeName = "DOM OperationNotAllowed";
    description->code = ec - OperationNotAllowedExceptionOffset;
    description->type = OperationNotAllowedExceptionType;

    size_t tableSize = WTF_ARRAY_LENGTH(operationNotAllowedExceptions);
    size_t tableIndex = ec - NOT_ALLOWED_ERR;

    description->name = tableIndex < tableSize ? operationNotAllowedExceptions[tableIndex].name : 0;
    description->description = tableIndex < tableSize ? operationNotAllowedExceptions[tableIndex].description : 0;

    return true;
}

} // namespace WebCore

#endif // ENABLE(BLOB)
