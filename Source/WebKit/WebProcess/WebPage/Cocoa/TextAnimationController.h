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
enum class TextAnimationRunMode : uint8_t;

namespace WritingTools {
using SessionID = WTF::UUID;
}

}

namespace WebKit {

class WebPage;

struct TextAnimationRange {
    WTF::UUID animationUUID;
    WebCore::CharacterRange range;
};

struct TextAnimationUnstyledRangeData {
    WTF::UUID animationUUID;
    WebCore::SimpleRange range;
};

struct ReplacedRangeAndString {
    WebCore::CharacterRange range;
    String string;
};

class TextAnimationController final {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(TextAnimationController);

public:
    explicit TextAnimationController(WebPage&);

    void removeTransparentMarkersForSessionID(const WTF::UUID& sessionUUID);
    void removeTransparentMarkersForTextAnimationID(const WTF::UUID&);

    void removeInitialTextAnimation(const WebCore::WritingTools::SessionID&);
    void addInitialTextAnimation(const WebCore::WritingTools::SessionID&);
    void addSourceTextAnimation(const WebCore::WritingTools::SessionID&, const WebCore::CharacterRange&, const String&, CompletionHandler<void(WebCore::TextAnimationRunMode)>&&);
    void addDestinationTextAnimation(const WebCore::WritingTools::SessionID&, const std::optional<WebCore::CharacterRange>&, const String&);

    void clearAnimationsForSessionID(const WebCore::WritingTools::SessionID&);

    void updateUnderlyingTextVisibilityForTextAnimationID(const WTF::UUID&, bool visible, CompletionHandler<void()>&&);

    void showSelectionForWritingToolsSessionAssociatedWithAnimationID(const WTF::UUID&);

    std::optional<WebCore::TextIndicatorData> createTextIndicatorForRange(const WebCore::SimpleRange&);
    void createTextIndicatorForTextAnimationID(const WTF::UUID&, CompletionHandler<void(std::optional<WebCore::TextIndicatorData>&&)>&&);

    void enableSourceTextAnimationAfterElementWithID(const String& elementID, const WTF::UUID&);
    void enableTextAnimationTypeForElementWithID(const String& elementID, const WTF::UUID&);

private:
    std::optional<WebCore::SimpleRange> contextRangeForTextAnimationID(const WTF::UUID&) const;
    std::optional<WebCore::SimpleRange> contextRangeForSessionWithID(const WebCore::WritingTools::SessionID&) const;
    std::optional<WebCore::SimpleRange> unreplacedRangeForSessionWithID(const WebCore::WritingTools::SessionID&) const;

    RefPtr<WebCore::Document> document() const;
    WeakPtr<WebPage> m_webPage;

    HashMap<WebCore::WritingTools::SessionID, WTF::UUID> m_initialAnimations;
    HashMap<WebCore::WritingTools::SessionID, Vector<TextAnimationRange>> m_textAnimationRanges;
    HashMap<WebCore::WritingTools::SessionID, std::optional<ReplacedRangeAndString>> m_alreadyReplacedRanges;
    HashMap<WebCore::WritingTools::SessionID, std::optional<TextAnimationUnstyledRangeData>> m_unstyledRanges;
    HashMap<WebCore::WritingTools::SessionID, Ref<WebCore::Range>> m_manuallyEnabledAnimationRanges;
};

} // namespace WebKit

#endif // ENABLE(WRITING_TOOLS)
