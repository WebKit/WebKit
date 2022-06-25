/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include <WebCore/FindOptions.h>
#include <wtf/EnumTraits.h>

namespace WebKit {

enum class FindOptions : uint16_t {
    CaseInsensitive = 1 << 0,
    AtWordStarts = 1 << 1,
    TreatMedialCapitalAsWordStart = 1 << 2,
    Backwards = 1 << 3,
    WrapAround = 1 << 4,
    ShowOverlay = 1 << 5,
    ShowFindIndicator = 1 << 6,
    ShowHighlight = 1 << 7,
    DetermineMatchIndex = 1 << 8,
    NoIndexChange = 1 << 9,
    AtWordEnds = 1 << 10,
};

enum class FindDecorationStyle : uint8_t {
    Normal,
    Found,
    Highlighted,
};

WebCore::FindOptions core(OptionSet<FindOptions>);

} // namespace WebKit

namespace WTF {

template<> struct EnumTraits<WebKit::FindOptions> {
    using values = EnumValues<
        WebKit::FindOptions,
        WebKit::FindOptions::CaseInsensitive,
        WebKit::FindOptions::AtWordStarts,
        WebKit::FindOptions::TreatMedialCapitalAsWordStart,
        WebKit::FindOptions::Backwards,
        WebKit::FindOptions::WrapAround,
        WebKit::FindOptions::ShowOverlay,
        WebKit::FindOptions::ShowFindIndicator,
        WebKit::FindOptions::ShowHighlight,
        WebKit::FindOptions::DetermineMatchIndex,
        WebKit::FindOptions::NoIndexChange,
        WebKit::FindOptions::AtWordEnds
    >;
};

template<> struct EnumTraits<WebKit::FindDecorationStyle> {
    using values = EnumValues<
        WebKit::FindDecorationStyle,
        WebKit::FindDecorationStyle::Normal,
        WebKit::FindDecorationStyle::Found,
        WebKit::FindDecorationStyle::Highlighted
    >;
};

} // namespace WTF
