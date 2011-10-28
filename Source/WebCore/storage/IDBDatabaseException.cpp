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

#if ENABLE(INDEXED_DATABASE)

#include "IDBDatabaseException.h"

namespace WebCore {

// FIXME: This should be an array of structs to pair the names and descriptions.
static const char* const exceptionNames[] = {
    "UNKNOWN_ERR",
    "NON_TRANSIENT_ERR",
    "NOT_FOUND_ERR",
    "CONSTRAINT_ERR",
    "DATA_ERR",
    "NOT_ALLOWED_ERR",
    "SERIAL_ERR",
    "RECOVERABLE_ERR",
    "TRANSIENT_ERR",
    "TIMEOUT_ERR",
    "DEADLOCK_ERR",
    "READ_ONLY_ERR",
    "ABORT_ERR"
};

static const char* const exceptionDescriptions[] = {
    "An unknown error occurred within Indexed Database.",
    "NON_TRANSIENT_ERR", // FIXME: Write a better message if it's ever possible this is thrown.
    "The name supplied does not match any existing item.",
    "The request cannot be completed due to a failed constraint.",
    "The data provided does not meet the requirements of the function.",
    "This function is not allowed to be called in such a context.",
    "The data supplied cannot be serialized according to the structured cloning algorithm.",
    "RECOVERABLE_ERR", // FIXME: This isn't even used.
    "TRANSIENT_ERR", // FIXME: This isn't even used.
    "TIMEOUT_ERR", // This can't be thrown.
    "DEADLOCK_ERR", // This can't be thrown.
    "Write operations cannot be preformed on a read-only transaction.",
    "The transaction was aborted, so the request cannot be fulfilled."
};

COMPILE_ASSERT(WTF_ARRAY_LENGTH(exceptionNames) == WTF_ARRAY_LENGTH(exceptionDescriptions), IDBDatabaseExceptionTablesMustMatch);

bool IDBDatabaseException::initializeDescription(ExceptionCode ec, ExceptionCodeDescription* description)
{
    if (ec < IDBDatabaseExceptionOffset || ec > IDBDatabaseExceptionMax)
        return false;

    description->typeName = "DOM IDBDatabase";
    description->code = ec - IDBDatabaseExceptionOffset;
    description->type = IDBDatabaseExceptionType;

    size_t tableSize = WTF_ARRAY_LENGTH(exceptionNames);
    size_t tableIndex = ec - UNKNOWN_ERR;

    description->name = tableIndex < tableSize ? exceptionNames[tableIndex] : 0;
    description->description = tableIndex < tableSize ? exceptionDescriptions[tableIndex] : 0;

    return true;
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
