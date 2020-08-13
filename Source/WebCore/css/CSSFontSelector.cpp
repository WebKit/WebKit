/*
 * Copyright (C) 2007, 2008, 2011, 2013 Apple Inc. All rights reserved.
 *           (C) 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
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

#include "config.h"
#include "CSSFontSelector.h"

#include "CachedFont.h"
#include "CSSFontFace.h"
#include "CSSFontFaceSource.h"
#include "CSSFontFamily.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyNames.h"
#include "CSSSegmentedFontFace.h"
#include "CSSValueKeywords.h"
#include "CSSValueList.h"
#include "CachedResourceLoader.h"
#include "Document.h"
#include "Font.h"
#include "FontCache.h"
#include "FontFace.h"
#include "FontFaceSet.h"
#include "FontSelectorClient.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "Logging.h"
#include "ResourceLoadObserver.h"
#include "RuntimeEnabledFeatures.h"
#include "Settings.h"
#include "StyleProperties.h"
#include "StyleResolver.h"
#include "StyleRule.h"
#include "WebKitFontFamilyNames.h"
#include <wtf/Ref.h>
#include <wtf/SetForScope.h>
#include <wtf/text/AtomString.h>

namespace WebCore {

static unsigned fontSelectorId;

CSSFontSelector::CSSFontSelector(Document& document)
    : m_document(makeWeakPtr(document))
    , m_cssFontFaceSet(CSSFontFaceSet::create(this))
    , m_beginLoadingTimer(&document, *this, &CSSFontSelector::beginLoadTimerFired)
    , m_uniqueId(++fontSelectorId)
    , m_version(0)
{
    ASSERT(m_document);
    FontCache::singleton().addClient(*this);
    m_cssFontFaceSet->addClient(*this);
    LOG(Fonts, "CSSFontSelector %p ctor", this);

    m_beginLoadingTimer.suspendIfNeeded();
}

CSSFontSelector::~CSSFontSelector()
{
    LOG(Fonts, "CSSFontSelector %p dtor", this);

    clearDocument();
    m_cssFontFaceSet->removeClient(*this);
    FontCache::singleton().removeClient(*this);
}

FontFaceSet* CSSFontSelector::fontFaceSetIfExists()
{
    return m_fontFaceSet.get();
}

FontFaceSet& CSSFontSelector::fontFaceSet()
{
    if (!m_fontFaceSet) {
        ASSERT(m_document);
        m_fontFaceSet = FontFaceSet::create(*m_document, m_cssFontFaceSet.get());
    }

    return *m_fontFaceSet;
}

bool CSSFontSelector::isEmpty() const
{
    return !m_cssFontFaceSet->faceCount();
}

void CSSFontSelector::emptyCaches()
{
    m_cssFontFaceSet->emptyCaches();
}

void CSSFontSelector::buildStarted()
{
    m_buildIsUnderway = true;
    m_cssFontFaceSet->purge();
    ++m_version;

    ASSERT(m_cssConnectionsPossiblyToRemove.isEmpty());
    ASSERT(m_cssConnectionsEncounteredDuringBuild.isEmpty());
    ASSERT(m_stagingArea.isEmpty());
    for (size_t i = 0; i < m_cssFontFaceSet->faceCount(); ++i) {
        CSSFontFace& face = m_cssFontFaceSet.get()[i];
        if (face.cssConnection())
            m_cssConnectionsPossiblyToRemove.add(&face);
    }
}

void CSSFontSelector::buildCompleted()
{
    if (!m_buildIsUnderway)
        return;

    m_buildIsUnderway = false;

    // Some font faces weren't re-added during the build process.
    for (auto& face : m_cssConnectionsPossiblyToRemove) {
        auto* connection = face->cssConnection();
        ASSERT(connection);
        if (!m_cssConnectionsEncounteredDuringBuild.contains(connection))
            m_cssFontFaceSet->remove(*face);
    }

    for (auto& item : m_stagingArea)
        addFontFaceRule(item.styleRuleFontFace, item.isInitiatingElementInUserAgentShadowTree);
    m_cssConnectionsEncounteredDuringBuild.clear();
    m_stagingArea.clear();
    m_cssConnectionsPossiblyToRemove.clear();
}

void CSSFontSelector::addFontFaceRule(StyleRuleFontFace& fontFaceRule, bool isInitiatingElementInUserAgentShadowTree)
{
    if (m_buildIsUnderway) {
        m_cssConnectionsEncounteredDuringBuild.add(&fontFaceRule);
        m_stagingArea.append({fontFaceRule, isInitiatingElementInUserAgentShadowTree});
        return;
    }

    const StyleProperties& style = fontFaceRule.properties();
    RefPtr<CSSValue> fontFamily = style.getPropertyCSSValue(CSSPropertyFontFamily);
    RefPtr<CSSValue> fontStyle = style.getPropertyCSSValue(CSSPropertyFontStyle);
    RefPtr<CSSValue> fontWeight = style.getPropertyCSSValue(CSSPropertyFontWeight);
    RefPtr<CSSValue> fontStretch = style.getPropertyCSSValue(CSSPropertyFontStretch);
    RefPtr<CSSValue> src = style.getPropertyCSSValue(CSSPropertySrc);
    RefPtr<CSSValue> unicodeRange = style.getPropertyCSSValue(CSSPropertyUnicodeRange);
    RefPtr<CSSValue> featureSettings = style.getPropertyCSSValue(CSSPropertyFontFeatureSettings);
    RefPtr<CSSValue> loadingBehavior = style.getPropertyCSSValue(CSSPropertyFontDisplay);
    if (!is<CSSValueList>(fontFamily) || !is<CSSValueList>(src) || (unicodeRange && !is<CSSValueList>(*unicodeRange)))
        return;

    CSSValueList& familyList = downcast<CSSValueList>(*fontFamily);
    if (!familyList.length())
        return;

    CSSValueList* rangeList = downcast<CSSValueList>(unicodeRange.get());

    CSSValueList& srcList = downcast<CSSValueList>(*src);
    if (!srcList.length())
        return;

    SetForScope<bool> creatingFont(m_creatingFont, true);
    Ref<CSSFontFace> fontFace = CSSFontFace::create(this, &fontFaceRule);

    if (!fontFace->setFamilies(*fontFamily))
        return;
    if (fontStyle)
        fontFace->setStyle(*fontStyle);
    if (fontWeight)
        fontFace->setWeight(*fontWeight);
    if (fontStretch)
        fontFace->setStretch(*fontStretch);
    if (rangeList && !fontFace->setUnicodeRange(*rangeList))
        return;
    if (featureSettings)
        fontFace->setFeatureSettings(*featureSettings);
    if (loadingBehavior)
        fontFace->setLoadingBehavior(*loadingBehavior);

    CSSFontFace::appendSources(fontFace, srcList, m_document.get(), isInitiatingElementInUserAgentShadowTree);
    if (fontFace->computeFailureState())
        return;

    if (RefPtr<CSSFontFace> existingFace = m_cssFontFaceSet->lookUpByCSSConnection(fontFaceRule)) {
        // This adoption is fairly subtle. Script can trigger a purge of m_cssFontFaceSet at any time,
        // which will cause us to just rely on the memory cache to retain the bytes of the file the next
        // time we build up the CSSFontFaceSet. However, when the CSS Font Loading API is involved,
        // the FontFace and FontFaceSet objects need to retain state. We create the new CSSFontFace object
        // while the old one is still in scope so that the memory cache will be forced to retain the bytes
        // of the resource. This means that the CachedFont will temporarily have two clients (until the
        // old CSSFontFace goes out of scope, which should happen at the end of this "if" block). Because
        // the CSSFontFaceSource objects will inspect their CachedFonts, the new CSSFontFace is smart enough
        // to enter the correct state() during the next pump(). This approach of making a new CSSFontFace is
        // simpler than computing and applying a diff of the StyleProperties.
        m_cssFontFaceSet->remove(*existingFace);
        if (auto* existingWrapper = existingFace->existingWrapper())
            existingWrapper->adopt(fontFace.get());
    }

    m_cssFontFaceSet->add(fontFace.get());
    ++m_version;
}

void CSSFontSelector::registerForInvalidationCallbacks(FontSelectorClient& client)
{
    m_clients.add(&client);
}

void CSSFontSelector::unregisterForInvalidationCallbacks(FontSelectorClient& client)
{
    m_clients.remove(&client);
}

void CSSFontSelector::dispatchInvalidationCallbacks()
{
    ++m_version;

    for (auto& client : copyToVector(m_clients))
        client->fontsNeedUpdate(*this);
}

void CSSFontSelector::opportunisticallyStartFontDataURLLoading(const FontCascadeDescription& description, const AtomString& familyName)
{
    const auto& segmentedFontFace = m_cssFontFaceSet->fontFace(description.fontSelectionRequest(), familyName);
    if (!segmentedFontFace)
        return;
    for (auto& face : segmentedFontFace->constituentFaces())
        face->opportunisticallyStartFontDataURLLoading(*this);
}

void CSSFontSelector::fontLoaded()
{
    dispatchInvalidationCallbacks();
}

void CSSFontSelector::fontModified()
{
    if (!m_creatingFont && !m_buildIsUnderway)
        dispatchInvalidationCallbacks();
}

void CSSFontSelector::fontCacheInvalidated()
{
    dispatchInvalidationCallbacks();
}

static Optional<AtomString> resolveGenericFamily(Document* document, const FontDescription& fontDescription, const AtomString& familyName)
{
    auto platformResult = FontDescription::platformResolveGenericFamily(fontDescription.script(), fontDescription.computedLocale(), familyName);
    if (!platformResult.isNull())
        return platformResult;

    if (!document)
        return WTF::nullopt;

    const Settings& settings = document->settings();

    UScriptCode script = fontDescription.script();
    if (familyName == serifFamily)
        return settings.serifFontFamily(script);
    if (familyName == sansSerifFamily)
        return settings.sansSerifFontFamily(script);
    if (familyName == cursiveFamily)
        return settings.cursiveFontFamily(script);
    if (familyName == fantasyFamily)
        return settings.fantasyFontFamily(script);
    if (familyName == monospaceFamily)
        return settings.fixedFontFamily(script);
    if (familyName == pictographFamily)
        return settings.pictographFontFamily(script);
    if (familyName == standardFamily)
        return settings.standardFontFamily(script);

    return WTF::nullopt;
}

FontRanges CSSFontSelector::fontRangesForFamily(const FontDescription& fontDescription, const AtomString& familyName)
{
    // If this ASSERT() fires, it usually means you forgot a document.updateStyleIfNeeded() somewhere.
    ASSERT(!m_buildIsUnderway || m_computingRootStyleFontCount);

    // FIXME: The spec (and Firefox) says user specified generic families (sans-serif etc.) should be resolved before the @font-face lookup too.
    bool resolveGenericFamilyFirst = familyName == standardFamily;

    AtomString familyForLookup = familyName;
    Optional<FontDescription> overrideFontDescription;
    const FontDescription* fontDescriptionForLookup = &fontDescription;
    auto resolveGenericFamily = [&]() {
        if (auto genericFamilyOptional = WebCore::resolveGenericFamily(m_document.get(), fontDescription, familyName))
            familyForLookup = *genericFamilyOptional;
    };

    if (resolveGenericFamilyFirst)
        resolveGenericFamily();
    auto* face = m_cssFontFaceSet->fontFace(fontDescriptionForLookup->fontSelectionRequest(), familyForLookup);
    if (face) {
        if (RuntimeEnabledFeatures::sharedFeatures().webAPIStatisticsEnabled()) {
            if (m_document)
                ResourceLoadObserver::shared().logFontLoad(*m_document, familyForLookup.string(), true);
        }
        return face->fontRanges(*fontDescriptionForLookup);
    }

    if (!resolveGenericFamilyFirst)
        resolveGenericFamily();
    auto font = FontCache::singleton().fontForFamily(*fontDescriptionForLookup, familyForLookup);
    if (RuntimeEnabledFeatures::sharedFeatures().webAPIStatisticsEnabled()) {
        if (m_document)
            ResourceLoadObserver::shared().logFontLoad(*m_document, familyForLookup.string(), !!font);
    }
    return FontRanges { WTFMove(font) };
}

void CSSFontSelector::clearDocument()
{
    if (!m_document) {
        ASSERT(!m_beginLoadingTimer.isActive());
        ASSERT(m_fontsToBeginLoading.isEmpty());
        return;
    }

    m_beginLoadingTimer.cancel();

    CachedResourceLoader& cachedResourceLoader = m_document->cachedResourceLoader();
    for (auto& fontHandle : m_fontsToBeginLoading) {
        // Balances incrementRequestCount() in beginLoadingFontSoon().
        cachedResourceLoader.decrementRequestCount(*fontHandle);
    }
    m_fontsToBeginLoading.clear();

    m_document = nullptr;

    m_cssFontFaceSet->clear();
    m_clients.clear();
}

void CSSFontSelector::beginLoadingFontSoon(CachedFont& font)
{
    if (!m_document)
        return;

    m_fontsToBeginLoading.append(&font);
    // Increment the request count now, in order to prevent didFinishLoad from being dispatched
    // after this font has been requested but before it began loading. Balanced by
    // decrementRequestCount() in beginLoadTimerFired() and in clearDocument().
    m_document->cachedResourceLoader().incrementRequestCount(font);

    if (!m_fontLoadingTimerIsSuspended)
        m_beginLoadingTimer.startOneShot(0_s);
}

void CSSFontSelector::suspendFontLoadingTimer()
{
    if (m_fontLoadingTimerIsSuspended)
        return;

    m_beginLoadingTimer.cancel();
    m_fontLoadingTimerIsSuspended = true;
}

void CSSFontSelector::restartFontLoadingTimer()
{
    if (!m_fontLoadingTimerIsSuspended)
        return;

    if (!m_fontsToBeginLoading.isEmpty())
        m_beginLoadingTimer.startOneShot(0_s);

    m_fontLoadingTimerIsSuspended = false;
}

void CSSFontSelector::beginLoadTimerFired()
{
    Vector<CachedResourceHandle<CachedFont>> fontsToBeginLoading;
    fontsToBeginLoading.swap(m_fontsToBeginLoading);

    // CSSFontSelector could get deleted via beginLoadIfNeeded() or loadDone() unless protected.
    Ref<CSSFontSelector> protectedThis(*this);

    CachedResourceLoader& cachedResourceLoader = m_document->cachedResourceLoader();
    for (auto& fontHandle : fontsToBeginLoading) {
        fontHandle->beginLoadIfNeeded(cachedResourceLoader);
        // Balances incrementRequestCount() in beginLoadingFontSoon().
        cachedResourceLoader.decrementRequestCount(*fontHandle);
    }
    // FIXME: Use SubresourceLoader instead.
    // Call FrameLoader::loadDone before FrameLoader::subresourceLoadDone to match the order in SubresourceLoader::notifyDone.
    cachedResourceLoader.loadDone(LoadCompletionType::Finish);
    // Ensure that if the request count reaches zero, the frame loader will know about it.
    // New font loads may be triggered by layout after the document load is complete but before we have dispatched
    // didFinishLoading for the frame. Make sure the delegate is always dispatched by checking explicitly.
    if (m_document && m_document->frame())
        m_document->frame()->loader().checkLoadComplete();
}


size_t CSSFontSelector::fallbackFontCount()
{
    if (!m_document)
        return 0;

    return m_document->settings().fontFallbackPrefersPictographs() ? 1 : 0;
}

RefPtr<Font> CSSFontSelector::fallbackFontAt(const FontDescription& fontDescription, size_t index)
{
    ASSERT_UNUSED(index, !index);

    if (!m_document)
        return nullptr;

    if (!m_document->settings().fontFallbackPrefersPictographs())
        return nullptr;
    auto& pictographFontFamily = m_document->settings().pictographFontFamily();
    auto font = FontCache::singleton().fontForFamily(fontDescription, pictographFontFamily);
    if (RuntimeEnabledFeatures::sharedFeatures().webAPIStatisticsEnabled())
        ResourceLoadObserver::shared().logFontLoad(*m_document, pictographFontFamily.string(), !!font);
    
    return font;
}

}
