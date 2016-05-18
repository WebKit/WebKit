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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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
#include "DOMCoreException.h"

#include "ExceptionCodeDescription.h"

namespace WebCore {

static const struct CoreException {
    const char* const name;
    const char* const description;
} coreExceptions[] = {
    { "IndexSizeError", "The index is not in the allowed range." },
    { 0, 0 }, // DOMStringSizeError
    { "HierarchyRequestError", "The operation would yield an incorrect node tree." },
    { "WrongDocumentError", "The object is in the wrong document." },
    { "InvalidCharacterError", "The string contains invalid characters." },
    { 0, 0 }, // NoDataAllowedError
    { "NoModificationAllowedError", "The object can not be modified." },
    { "NotFoundError", "The object can not be found here." },
    { "NotSupportedError", "The operation is not supported." },
    { "InUseAttributeError", "The attribute is in use." },
    { "InvalidStateError", "The object is in an invalid state." },
    { "SyntaxError", "The string did not match the expected pattern." },
    { "InvalidModificationError", " The object can not be modified in this way." },
    { "NamespaceError", "The operation is not allowed by Namespaces in XML." },
    { "InvalidAccessError", "The object does not support the operation or argument." },
    { 0, 0 }, // ValidationError
    { "TypeMismatchError", "The type of an object was incompatible with the expected type of the parameter associated to the object." },
    { "SecurityError", "The operation is insecure." },
    { "NetworkError", " A network error occurred." },
    { "AbortError", "The operation was aborted." },
    { "URLMismatchError", "The given URL does not match another URL." },
    { "QuotaExceededError", "The quota has been exceeded." },
    { "TimeoutError", "The operation timed out." },
    { "InvalidNodeTypeError", "The supplied node is incorrect or has an incorrect ancestor for this operation." },
    { "DataCloneError", "The object can not be cloned." },
    { "EncodingError", "The encoding operation (either encoded or decoding) failed." },
    { "NotReadableError", "The I/O read operation failed." },
    { "UnknownError", "The operation failed for an unknown transient reason (e.g. out of memory)." },
    { "ConstraintError", "A mutation operation in a transaction failed because a constraint was not satisfied." },
    { "DataError", "Provided data is inadequate." },
    { "TransactionInactiveError", "A request was placed against a transaction which is currently not active, or which is finished." },
    { "ReadOnlyError", "The mutating operation was attempted in a \"readonly\" transaction." },
    { "VersionError", "An attempt was made to open a database using a lower version than the existing version." },
    { "OperationError", "The operation failed for an operation-specific reason." },
    { "NotAllowedError", "The request is not allowed by the user agent or the platform in the current context, possibly because the user denied permission." }
};

bool DOMCoreException::initializeDescription(ExceptionCode ec, ExceptionCodeDescription* description)
{
    description->typeName = "DOM";
    description->code = ec;
    description->type = DOMCoreExceptionType;

    size_t tableSize = WTF_ARRAY_LENGTH(coreExceptions);
    size_t tableIndex = ec - INDEX_SIZE_ERR;

    description->name = tableIndex < tableSize ? coreExceptions[tableIndex].name : 0;
    description->description = tableIndex < tableSize ? coreExceptions[tableIndex].description : 0;

    return true;
}

} // namespace WebCore
