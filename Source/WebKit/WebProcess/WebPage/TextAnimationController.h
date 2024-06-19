/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#if ENABLE(WRITING_TOOLS_UI)

#include <WebCore/CharacterRange.h>
#include <WebCore/SimpleRange.h>
#include <wtf/FastMalloc.h>
#include <wtf/Noncopyable.h>
#include <wtf/UUID.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class Range;

struct TextIndicatorData;

enum class TextIndicatorOption : uint16_t;

namespace WritingTools {
using SessionID = WTF::UUID;
}

}

namespace WebKit {

class WebPage;

struct TextAnimationState {
    WTF::UUID styleID;
    WebCore::CharacterRange range;
};

struct TextAnimationUnstyledRangeData {
    WTF::UUID styleID;
    WebCore::SimpleRange range;
};

class TextAnimationController final {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(TextAnimationController);

public:
    explicit TextAnimationController(WebPage&);

    void cleanUpTextAnimationsForSessionID(const WTF::UUID& sessionUUID);
    void removeTransparentMarkersForTextAnimationID(const WTF::UUID&);

    void addSourceTextAnimation(const WTF::UUID& sessionUUID, const WebCore::CharacterRange&);
    void addDestinationTextAnimation(const WTF::UUID& sessionUUID, const WebCore::CharacterRange&);

    void updateUnderlyingTextVisibilityForTextAnimationID(const WTF::UUID&, bool visible, CompletionHandler<void()>&&);

    void createTextIndicatorForRange(const WebCore::SimpleRange&, CompletionHandler<void(std::optional<WebCore::TextIndicatorData>&&)>&&);
    void createTextIndicatorForTextAnimationID(const WTF::UUID&, CompletionHandler<void(std::optional<WebCore::TextIndicatorData>&&)>&&);

    void enableSourceTextAnimationAfterElementWithID(const String& elementID, const WTF::UUID&);
    void enableTextAnimationTypeForElementWithID(const String& elementID, const WTF::UUID&);

private:
    std::optional<WebCore::SimpleRange> contextRangeForTextAnimationType(const WTF::UUID&) const;
    std::optional<WebCore::SimpleRange> contextRangeForSessionWithID(const WebCore::WritingTools::SessionID&) const;

    RefPtr<WebCore::Document> document() const;
    WeakPtr<WebPage> m_webPage;

    HashMap<WTF::UUID, Vector<TextAnimationState>> m_activeTextAnimations;
    HashMap<WTF::UUID, WebCore::CharacterRange> m_alreadyReplacedRanges;
    HashMap<WTF::UUID, std::optional<TextAnimationUnstyledRangeData>> m_unstyledRanges;
    HashMap<WTF::UUID, Ref<WebCore::Range>> m_manuallyEnabledAnimationRanges;
};

} // namespace WebKit

#endif // ENABLE(WRITING_TOOLS)
