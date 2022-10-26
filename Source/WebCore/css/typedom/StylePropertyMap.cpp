/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "StylePropertyMap.h"

#include "CSSPropertyNames.h"
#include "CSSPropertyParser.h"

namespace WebCore {

// https://drafts.css-houdini.org/css-typed-om/#dom-stylepropertymap-set
ExceptionOr<void> StylePropertyMap::set(Document&, const AtomString&, FixedVector<std::variant<RefPtr<CSSStyleValue>, String>>&&)
{
    return Exception { NotSupportedError, "set() is not yet supported"_s };
}

// https://drafts.css-houdini.org/css-typed-om/#dom-stylepropertymap-append
ExceptionOr<void> StylePropertyMap::append(Document&, const AtomString&, FixedVector<std::variant<RefPtr<CSSStyleValue>, String>>&&)
{
    return Exception { NotSupportedError, "append() is not yet supported"_s };
}

// https://drafts.css-houdini.org/css-typed-om/#dom-stylepropertymap-delete
ExceptionOr<void> StylePropertyMap::remove(Document& document, const AtomString& property)
{
    if (isCustomPropertyName(property)) {
        removeCustomProperty(property);
        return { };
    }

    auto propertyID = cssPropertyID(property);
    if (propertyID == CSSPropertyInvalid || !isCSSPropertyExposed(propertyID, &document.settings()))
        return Exception { TypeError, makeString("Invalid property ", property) };

    removeProperty(propertyID);
    return { };
}

} // namespace WebCore
