/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include <wtf/text/StringView.h>

#if USE(CF)

#include <CoreFoundation/CoreFoundation.h>
#include <wtf/RetainPtr.h>

namespace WTF {

RetainPtr<CFStringRef> StringView::createCFString() const
{
    if (is8Bit()) {
        auto characters = span8();
        return adoptCF(CFStringCreateWithBytes(kCFAllocatorDefault, characters.data(), characters.size(), kCFStringEncodingISOLatin1, false));
    }
    auto characters = span16();
    return adoptCF(CFStringCreateWithCharacters(kCFAllocatorDefault, reinterpret_cast<const UniChar*>(characters.data()), characters.size()));
}

RetainPtr<CFStringRef> StringView::createCFStringWithoutCopying() const
{
    if (is8Bit()) {
        auto characters = span8();
        return adoptCF(CFStringCreateWithBytesNoCopy(kCFAllocatorDefault, characters.data(), characters.size(), kCFStringEncodingISOLatin1, false, kCFAllocatorNull));
    }
    auto characters = span16();
    return adoptCF(CFStringCreateWithCharactersNoCopy(kCFAllocatorDefault, reinterpret_cast<const UniChar*>(characters.data()), characters.size(), kCFAllocatorNull));
}

}

#endif // USE(CF)
