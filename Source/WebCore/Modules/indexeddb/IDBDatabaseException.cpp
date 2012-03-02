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
    "TRANSACTION_INACTIVE_ERR",
    "ABORT_ERR",
    "READ_ONLY_ERR",
    "TIMEOUT_ERR",
    "QUOTA_ERR",
    "VER_ERR"
};

static const char* const exceptionDescriptions[] = {
    "An unknown error occurred within Indexed Database.",
    "NON_TRANSIENT_ERR", // FIXME: Write a better message if it's ever possible this is thrown.
    "The name supplied does not match any existing item.",
    "The request cannot be completed due to a failed constraint.",
    "The data provided does not meet the requirements of the function.",
    "This function is not allowed to be called in such a context.",
    "A request was placed against a transaction which is either currently not active, or which is finished.",
    "The transaction was aborted, so the request cannot be fulfilled.",
    "A write operation was attempted in a read-only transaction.",
    "A lock for the transaction could not be obtained in a reasonable time.", // FIXME: This isn't used yet.
    "The operation failed because there was not enough remaining storage space, or the storage quota was reached and the user declined to give more space to the database.", // FIXME: This isn't used yet
    "An attempt was made to open a database using a lower version than the existing version.", // FIXME: This isn't used yet
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
