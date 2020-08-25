/*
 * Copyright (C) 2007, 2008, 2011 Apple Inc. All rights reserved.
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

#pragma once

#include "ActiveDOMObject.h"
#include "CSSFontFace.h"
#include "CSSFontFaceSet.h"
#include "CachedResourceHandle.h"
#include "Font.h"
#include "FontSelector.h"
#include "Timer.h"
#include <memory>
#include <wtf/Forward.h>
#include <wtf/HashSet.h>
#include <wtf/RefPtr.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

class CSSFontFaceRule;
class CSSPrimitiveValue;
class CSSSegmentedFontFace;
class CSSValueList;
class CachedFont;
class Document;
class StyleRuleFontFace;

class CSSFontSelector final : public FontSelector, public CSSFontFaceSetClient, public CanMakeWeakPtr<CSSFontSelector>, public ActiveDOMObject {
public:
    static Ref<CSSFontSelector> create(Document& document)
    {
        return adoptRef(*new CSSFontSelector(document));
    }
    virtual ~CSSFontSelector();
    
    unsigned version() const final { return m_version; }
    unsigned uniqueId() const final { return m_uniqueId; }

    FontRanges fontRangesForFamily(const FontDescription&, const AtomString&) final;
    size_t fallbackFontCount() final;
    RefPtr<Font> fallbackFontAt(const FontDescription&, size_t) final;

    void clearDocument();
    void emptyCaches();
    void buildStarted();
    void buildCompleted();

    void addFontFaceRule(StyleRuleFontFace&, bool isInitiatingElementInUserAgentShadowTree);

    void fontLoaded();
    void fontCacheInvalidated() final;

    bool isEmpty() const;

    void registerForInvalidationCallbacks(FontSelectorClient&) final;
    void unregisterForInvalidationCallbacks(FontSelectorClient&) final;

    Document* document() const { return m_document.get(); }

    void beginLoadingFontSoon(CachedFont&);
    void suspendFontLoadingTimer();

    FontFaceSet* fontFaceSetIfExists();
    FontFaceSet& fontFaceSet();

    void incrementIsComputingRootStyleFont() { ++m_computingRootStyleFontCount; }
    void decrementIsComputingRootStyleFont() { --m_computingRootStyleFontCount; }

private:
    explicit CSSFontSelector(Document&);

    void dispatchInvalidationCallbacks();

    void opportunisticallyStartFontDataURLLoading(const FontCascadeDescription&, const AtomString& family) final;

    void fontModified() final;

    void fontLoadingTimerFired();

    // ActiveDOMObject
    void stop() final;
    void suspend(ReasonForSuspension) final;
    void resume() final;
    const char* activeDOMObjectName() const final { return "CSSFontSelector"_s; }

    struct PendingFontFaceRule {
        StyleRuleFontFace& styleRuleFontFace;
        bool isInitiatingElementInUserAgentShadowTree;
    };
    Vector<PendingFontFaceRule> m_stagingArea;

    WeakPtr<Document> m_document;
    RefPtr<FontFaceSet> m_fontFaceSet;
    Ref<CSSFontFaceSet> m_cssFontFaceSet;
    HashSet<FontSelectorClient*> m_clients;

    Vector<CachedResourceHandle<CachedFont>> m_fontsToBeginLoading;
    HashSet<RefPtr<CSSFontFace>> m_cssConnectionsPossiblyToRemove;
    HashSet<RefPtr<StyleRuleFontFace>> m_cssConnectionsEncounteredDuringBuild;

    Timer m_fontLoadingTimer;
    bool m_fontLoadingTimerIsSuspended { false };

    unsigned m_uniqueId;
    unsigned m_version;
    unsigned m_computingRootStyleFontCount { 0 };
    bool m_creatingFont { false };
    bool m_buildIsUnderway { false };
};

} // namespace WebCore
