/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "EnterKeyHint.h"

namespace WebCore {

EnterKeyHint enterKeyHintForAttributeValue(const String& value)
{
    if (equalIgnoringASCIICase(value, "enter"))
        return EnterKeyHint::Enter;
    if (equalIgnoringASCIICase(value, "done"))
        return EnterKeyHint::Done;
    if (equalIgnoringASCIICase(value, "go"))
        return EnterKeyHint::Go;
    if (equalIgnoringASCIICase(value, "next"))
        return EnterKeyHint::Next;
    if (equalIgnoringASCIICase(value, "previous"))
        return EnterKeyHint::Previous;
    if (equalIgnoringASCIICase(value, "search"))
        return EnterKeyHint::Search;
    if (equalIgnoringASCIICase(value, "send"))
        return EnterKeyHint::Send;
    return EnterKeyHint::Unspecified;
}

String attributeValueForEnterKeyHint(EnterKeyHint hint)
{
    switch (hint) {
    case EnterKeyHint::Unspecified:
        return emptyAtom();
    case EnterKeyHint::Enter:
        return "enter";
    case EnterKeyHint::Done:
        return "done";
    case EnterKeyHint::Go:
        return "go";
    case EnterKeyHint::Next:
        return "next";
    case EnterKeyHint::Previous:
        return "previous";
    case EnterKeyHint::Search:
        return "search";
    case EnterKeyHint::Send:
        return "send";
    }
    ASSERT_NOT_REACHED();
    return nullAtom();
}

} // namespace WebCore
