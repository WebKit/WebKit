/*

Copyright (C) 2014 Apple Inc. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1.  Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
2.  Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#include "config.h"
#include "StringView.h"

#include <mutex>
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>

#if CHECK_STRINGVIEW_LIFETIME

namespace WTF {

// Manage reference count manually so UnderlyingString does not need to be defined in the header.

struct StringView::UnderlyingString {
    std::atomic_uint refCount { 1u };
    bool isValid { true };
    const StringImpl& string;
    explicit UnderlyingString(const StringImpl&);
};

StringView::UnderlyingString::UnderlyingString(const StringImpl& string)
    : string(string)
{
}

static std::mutex& underlyingStringsMutex()
{
    static NeverDestroyed<std::mutex> mutex;
    return mutex;
}

static HashMap<const StringImpl*, StringView::UnderlyingString*>& underlyingStrings()
{
    static NeverDestroyed<HashMap<const StringImpl*, StringView::UnderlyingString*>> map;
    return map;
}

void StringView::invalidate(const StringImpl& stringToBeDestroyed)
{
    UnderlyingString* underlyingString;
    {
        std::lock_guard<std::mutex> lock(underlyingStringsMutex());
        underlyingString = underlyingStrings().take(&stringToBeDestroyed);
        if (!underlyingString)
            return;
    }
    ASSERT(underlyingString->isValid);
    underlyingString->isValid = false;
}

bool StringView::underlyingStringIsValid() const
{
    return !m_underlyingString || m_underlyingString->isValid;
}

void StringView::adoptUnderlyingString(UnderlyingString* underlyingString)
{
    if (m_underlyingString) {
        if (!--m_underlyingString->refCount) {
            if (m_underlyingString->isValid) {
                std::lock_guard<std::mutex> lock(underlyingStringsMutex());
                underlyingStrings().remove(&m_underlyingString->string);
            }
            delete m_underlyingString;
        }
    }
    m_underlyingString = underlyingString;
}

void StringView::setUnderlyingString(const StringImpl* string)
{
    UnderlyingString* underlyingString;
    if (!string)
        underlyingString = nullptr;
    else {
        std::lock_guard<std::mutex> lock(underlyingStringsMutex());
        auto result = underlyingStrings().add(string, nullptr);
        if (result.isNewEntry)
            result.iterator->value = new UnderlyingString(*string);
        else
            ++result.iterator->value->refCount;
        underlyingString = result.iterator->value;
    }
    adoptUnderlyingString(underlyingString);
}

void StringView::setUnderlyingString(const StringView& string)
{
    UnderlyingString* underlyingString = string.m_underlyingString;
    if (underlyingString)
        ++underlyingString->refCount;
    adoptUnderlyingString(underlyingString);
}

bool StringView::startsWith(const StringView& prefix) const
{
    return ::WTF::startsWith(*this, prefix);
}

bool StringView::startsWithIgnoringASCIICase(const StringView& prefix) const
{
    return ::WTF::endsWithIgnoringASCIICase(*this, prefix);
}

bool StringView::endsWith(const StringView& prefix) const
{
    return ::WTF::startsWith(*this, prefix);
}

bool StringView::endsWithIgnoringASCIICase(const StringView& prefix) const
{
    return ::WTF::endsWithIgnoringASCIICase(*this, prefix);
}

}

#endif
