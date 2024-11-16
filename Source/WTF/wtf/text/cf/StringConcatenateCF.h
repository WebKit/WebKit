/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#pragma once

#include <wtf/text/StringConcatenate.h>

#if USE(CF)

namespace WTF {

template<> class StringTypeAdapter<CFStringRef> {
public:
    StringTypeAdapter(CFStringRef);
    unsigned length() const { return m_string ? CFStringGetLength(m_string) : 0; }
    bool is8Bit() const { return !m_string || CFStringGetCStringPtr(m_string, kCFStringEncodingISOLatin1); }
    template<typename CharacterType> void writeTo(std::span<CharacterType>) const;

private:
    CFStringRef m_string;
};

inline StringTypeAdapter<CFStringRef>::StringTypeAdapter(CFStringRef string)
    : m_string { string }
{
}

template<> inline void StringTypeAdapter<CFStringRef>::writeTo<LChar>(std::span<LChar> destination) const
{
    if (m_string)
        std::memcpy(destination.data(), CFStringGetCStringPtr(m_string, kCFStringEncodingISOLatin1), CFStringGetLength(m_string));
}

template<> inline void StringTypeAdapter<CFStringRef>::writeTo<UChar>(std::span<UChar> destination) const
{
    if (m_string)
        CFStringGetCharacters(m_string, CFRangeMake(0, CFStringGetLength(m_string)), reinterpret_cast<UniChar*>(destination.data()));
}

#ifdef __OBJC__

template<> class StringTypeAdapter<NSString *> : public StringTypeAdapter<CFStringRef> {
public:
    StringTypeAdapter(NSString *string)
        : StringTypeAdapter<CFStringRef>((__bridge CFStringRef)string)
    {
    }
};

#endif

}

#endif // USE(CF)
