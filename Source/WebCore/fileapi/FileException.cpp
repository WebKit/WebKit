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

#if ENABLE(BLOB) || ENABLE(FILE_SYSTEM)

#include "FileException.h"

namespace WebCore {

// FIXME: This should be an array of structs to pair the names and descriptions.
static const char* const exceptionNames[] = {
    "NOT_FOUND_ERR",
    "SECURITY_ERR",
    "ABORT_ERR",
    "NOT_READABLE_ERR",
    "ENCODING_ERR",
    "NO_MODIFICATION_ALLOWED_ERR",
    "INVALID_STATE_ERR",
    "SYNTAX_ERR",
    "INVALID_MODIFICATION_ERR",
    "QUOTA_EXCEEDED_ERR",
    "TYPE_MISMATCH_ERR",
    "PATH_EXISTS_ERR"
};

static const char* const exceptionDescriptions[] = {
    "A requested file or directory could not be found at the time an operation was processed.",
    "It was determined that certain files are unsafe for access within a Web application, or that too many calls are being made on file resources.",
    "An ongoing operation was aborted, typically with a call to abort().",
    "The requested file could not be read, typically due to permission problems that have occurred after a reference to a file was acquired.",
    "A URI supplied to the API was malformed, or the resulting Data URL has exceeded the URL length limitations for Data URLs.",
    "An attempt was made to write to a file or directory which could not be modified due to the state of the underlying filesystem.",
    "An operation that depends on state cached in an interface object was made but the state had changed since it was read from disk.",
    "An invalid or unsupported argument was given, like an invalid line ending specifier.",
    "The modification request was illegal.",
    "The operation failed because it would cause the application to exceed its storage quota.",
    "The path supplied exists, but was not an entry of requested type.",
    "An attempt was made to create a file or directory where an element already exists."
};

COMPILE_ASSERT(WTF_ARRAY_LENGTH(exceptionNames) == WTF_ARRAY_LENGTH(exceptionDescriptions), FileExceptionTablesMustMatch);

bool FileException::initializeDescription(ExceptionCode ec, ExceptionCodeDescription* description)
{
    if (ec < FileExceptionOffset || ec > FileExceptionMax)
        return false;

    description->typeName = "DOM File";
    description->code = ec - FileExceptionOffset;
    description->type = FileExceptionType;

    size_t tableSize = WTF_ARRAY_LENGTH(exceptionNames);
    size_t tableIndex = ec - NOT_FOUND_ERR;

    description->name = tableIndex < tableSize ? exceptionNames[tableIndex] : 0;
    description->description = tableIndex < tableSize ? exceptionDescriptions[tableIndex] : 0;

    return true;
}

} // namespace WebCore

#endif // ENABLE(BLOB) || ENABLE(FILE_SYSTEM)
