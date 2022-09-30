/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "HTTPHeaderField.h"

#include "RFC7230.h"

namespace WebCore {

std::optional<HTTPHeaderField> HTTPHeaderField::create(String&& unparsedName, String&& unparsedValue)
{
    StringView strippedName = StringView(unparsedName).stripLeadingAndTrailingMatchedCharacters(RFC7230::isWhitespace<UChar>);
    StringView strippedValue = StringView(unparsedValue).stripLeadingAndTrailingMatchedCharacters(RFC7230::isWhitespace<UChar>);
    if (!RFC7230::isValidName(strippedName) || !RFC7230::isValidValue(strippedValue))
        return std::nullopt;

    String name = strippedName.length() == unparsedName.length() ? WTFMove(unparsedName) : strippedName.toString();
    String value = strippedValue.length() == unparsedValue.length() ? WTFMove(unparsedValue) : strippedValue.toString();
    return {{ WTFMove(name), WTFMove(value) }};
}

}
