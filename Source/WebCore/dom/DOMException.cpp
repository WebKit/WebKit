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
#include "DOMException.h"

#include "Exception.h"

namespace WebCore {

// This array needs to be kept in sync with the ExceptionCode enumeration.
// http://heycam.github.io/webidl/#idl-DOMException-error-names
static const DOMException::Description descriptions[] = {
    { "IndexSizeError", "The index is not in the allowed range.", 1 },
    { "HierarchyRequestError", "The operation would yield an incorrect node tree.", 3 },
    { "WrongDocumentError", "The object is in the wrong document.", 4 },
    { "InvalidCharacterError", "The string contains invalid characters.", 5 },
    { "NoModificationAllowedError", "The object can not be modified.", 7 },
    { "NotFoundError", "The object can not be found here.", 8 },
    { "NotSupportedError", "The operation is not supported.", 9 },
    { "InUseAttributeError", "The attribute is in use.", 10 },
    { "InvalidStateError", "The object is in an invalid state.", 11 },
    { "SyntaxError", "The string did not match the expected pattern.", 12 },
    { "InvalidModificationError", " The object can not be modified in this way.", 13 },
    { "NamespaceError", "The operation is not allowed by Namespaces in XML.", 14 },
    { "InvalidAccessError", "The object does not support the operation or argument.", 15 },
    { "TypeMismatchError", "The type of an object was incompatible with the expected type of the parameter associated to the object.", 17 },
    { "SecurityError", "The operation is insecure.", 18 },
    { "NetworkError", " A network error occurred.", 19 },
    { "AbortError", "The operation was aborted.", 20 },
    { "URLMismatchError", "The given URL does not match another URL.", 21 },
    { "QuotaExceededError", "The quota has been exceeded.", 22 },
    { "TimeoutError", "The operation timed out.", 23 },
    { "InvalidNodeTypeError", "The supplied node is incorrect or has an incorrect ancestor for this operation.", 24 },
    { "DataCloneError", "The object can not be cloned.", 25 },
    { "EncodingError", "The encoding operation (either encoded or decoding) failed.", 0 },
    { "NotReadableError", "The I/O read operation failed.", 0 },
    { "UnknownError", "The operation failed for an unknown transient reason (e.g. out of memory).", 0 },
    { "ConstraintError", "A mutation operation in a transaction failed because a constraint was not satisfied.", 0 },
    { "DataError", "Provided data is inadequate.", 0 },
    { "TransactionInactiveError", "A request was placed against a transaction which is currently not active, or which is finished.", 0 },
    { "ReadOnlyError", "The mutating operation was attempted in a \"readonly\" transaction.", 0 },
    { "VersionError", "An attempt was made to open a database using a lower version than the existing version.", 0 },
    { "OperationError", "The operation failed for an operation-specific reason.", 0 },
    { "NotAllowedError", "The request is not allowed by the user agent or the platform in the current context, possibly because the user denied permission.", 0 }
};
static_assert(!IndexSizeError, "This table needs to be kept in sync with DOMException names in ExceptionCode enumeration");
static_assert(NotAllowedError == WTF_ARRAY_LENGTH(descriptions) - 1, "This table needs to be kept in sync with DOMException names in ExceptionCode enumeration");

auto DOMException::description(ExceptionCode ec) -> const Description&
{
    if (ec < WTF_ARRAY_LENGTH(descriptions))
        return descriptions[ec];

    static const Description emptyDescription { nullptr, nullptr, 0 };
    return emptyDescription;
}

static DOMException::LegacyCode legacyCodeFromName(const String& name)
{
    for (auto& description : descriptions) {
        if (description.name == name)
            return description.legacyCode;
    }
    return 0;
}

Ref<DOMException> DOMException::create(ExceptionCode ec, const String& message)
{
    auto& description = DOMException::description(ec);
    return adoptRef(*new DOMException(description.legacyCode, description.name, !message.isEmpty() ? message : ASCIILiteral(description.message)));
}

Ref<DOMException> DOMException::create(const String& message, const String& name)
{
    return adoptRef(*new DOMException(legacyCodeFromName(name), name, message));
}

Ref<DOMException> DOMException::create(const Exception& exception)
{
    auto& description = DOMException::description(exception.code());
    return adoptRef(*new DOMException(description.legacyCode, description.name, exception.message().isEmpty() ? description.message : exception.message()));
}

DOMException::DOMException(LegacyCode legacyCode, const String& name, const String& message)
    : m_legacyCode(legacyCode)
    , m_name(name)
    , m_message(message)
{
}

} // namespace WebCore
