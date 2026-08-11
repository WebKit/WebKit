/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#if ENABLE(WRITING_TOOLS_TEXT_EFFECTS)

#include <WebCore/SimpleRange.h>
#include <WebCore/TextAnimationTypes.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Noncopyable.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/UUID.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class Document;
class Page;
class TextIndicator;

struct SimpleRange;

enum class IncludeDocumentMarkers : bool { No, Yes };

class TextEffectController final {
    WTF_MAKE_TZONE_ALLOCATED(TextEffectController);
    WTF_MAKE_NONCOPYABLE(TextEffectController);

public:
    explicit TextEffectController(Page&);

    void addTextEffect(const SimpleRange&, RefPtr<TextIndicator>&&, RefPtr<TextIndicator>&& decorationIndicator);
    void removeTextEffect(const SimpleRange&);
    void removeAllTextEffects();

    RefPtr<TextIndicator> createTextIndicatorForRange(const SimpleRange&, IncludeDocumentMarkers = IncludeDocumentMarkers::No);

    WEBCORE_EXPORT void updateUnderlyingTextVisibilityForTextEffectID(const WTF::UUID&, bool visible, CompletionHandler<void()>&&);
    WEBCORE_EXPORT void createTextIndicatorForTextEffectID(const WTF::UUID&, CompletionHandler<void(RefPtr<TextIndicator>&&)>&&);

private:
    RefPtr<Document> document() const;
    std::optional<SimpleRange> rangeForTextEffectID(const WTF::UUID&) const;
    void removeTransparentMarkersForTextEffectID(const WTF::UUID&);

    struct ActiveEffect {
        WTF::UUID uuid;
        SimpleRange range;
    };

    WeakPtr<Page> m_page;
    Vector<ActiveEffect> m_activeEffects;
};

} // namespace WebCore

#endif // ENABLE(WRITING_TOOLS_TEXT_EFFECTS)
