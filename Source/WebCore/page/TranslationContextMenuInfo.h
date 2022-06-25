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

#if HAVE(TRANSLATION_UI_SERVICES) && ENABLE(CONTEXT_MENUS)

#include "IntPoint.h"
#include "IntRect.h"
#include <wtf/EnumTraits.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

enum class TranslationContextMenuMode : bool { NonEditable, Editable };

struct TranslationContextMenuInfo {
    String text;
    IntRect selectionBoundsInRootView;
    IntPoint locationInRootView;
    TranslationContextMenuMode mode { TranslationContextMenuMode::NonEditable };

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<TranslationContextMenuInfo> decode(Decoder&);
};

template<class Encoder> void TranslationContextMenuInfo::encode(Encoder& encoder) const
{
    encoder << text;
    encoder << selectionBoundsInRootView;
    encoder << locationInRootView;
    encoder << mode;
}

template<class Decoder> std::optional<TranslationContextMenuInfo> TranslationContextMenuInfo::decode(Decoder& decoder)
{
    std::optional<String> text;
    decoder >> text;
    if (!text)
        return std::nullopt;

    std::optional<IntRect> selectionBoundsInRootView;
    decoder >> selectionBoundsInRootView;
    if (!selectionBoundsInRootView)
        return std::nullopt;

    std::optional<IntPoint> locationInRootView;
    decoder >> locationInRootView;
    if (!locationInRootView)
        return std::nullopt;

    std::optional<TranslationContextMenuMode> mode;
    decoder >> mode;
    if (!mode)
        return std::nullopt;

    return {{ WTFMove(*text), WTFMove(*selectionBoundsInRootView), WTFMove(*locationInRootView), *mode }};
}

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::TranslationContextMenuMode> {
    using values = EnumValues<
        WebCore::TranslationContextMenuMode,
        WebCore::TranslationContextMenuMode::NonEditable,
        WebCore::TranslationContextMenuMode::Editable
    >;
};

} // namespace WTF

#endif // HAVE(TRANSLATION_UI_SERVICES) && ENABLE(CONTEXT_MENUS)
