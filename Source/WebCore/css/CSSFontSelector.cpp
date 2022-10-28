/*
 * Copyright (C) 2007-2021 Apple Inc. All rights reserved.
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
#include "CSSFontFeatureValuesRule.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyNames.h"
#include "CSSSegmentedFontFace.h"
#include "CSSValueKeywords.h"
#include "CSSValueList.h"
#include "CachedResourceLoader.h"
#include "DeprecatedGlobalSettings.h"
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
#include "Settings.h"
#include "StyleProperties.h"
#include "StyleResolver.h"
#include "StyleRule.h"
#include <wtf/Ref.h>
#include <wtf/SetForScope.h>
#include <wtf/text/AtomString.h>

namespace WebCore {

using namespace WebKitFontFamilyNames;

static unsigned fontSelectorId;

Ref<CSSFontSelector> CSSFontSelector::create(ScriptExecutionContext& context)
{
    auto fontSelector = adoptRef(*new CSSFontSelector(context));
    fontSelector->suspendIfNeeded();
    return fontSelector;
}

CSSFontSelector::CSSFontSelector(ScriptExecutionContext& context)
    : ActiveDOMObject(&context)
    , m_context(context)
    , m_cssFontFaceSet(CSSFontFaceSet::create(this))
    , m_fontModifiedObserver([this] { fontModified(); })
    , m_uniqueId(++fontSelectorId)
    , m_version(0)
{
    if (is<Document>(context)) {
        m_fontFamilyNames.reserveInitialCapacity(familyNames->size());
        for (auto& familyName : familyNames.get())
            m_fontFamilyNames.uncheckedConstructAndAppend(familyName);
    } else {
        m_fontFamilyNames.reserveInitialCapacity(familyNamesData->size());
        for (auto& familyName : familyNamesData.get())
            m_fontFamilyNames.uncheckedAppend(familyName);
    }

    FontCache::forCurrentThread().addClient(*this);
    m_cssFontFaceSet->addFontModifiedObserver(m_fontModifiedObserver);
    LOG(Fonts, "CSSFontSelector %p ctor", this);
}

CSSFontSelector::~CSSFontSelector()
{
    LOG(Fonts, "CSSFontSelector %p dtor", this);

    clearFonts();

    if (auto fontCache = FontCache::forCurrentThreadIfExists())
        fontCache->removeClient(*this);
}

FontFaceSet* CSSFontSelector::fontFaceSetIfExists()
{
    return m_fontFaceSet.get();
}

FontFaceSet& CSSFontSelector::fontFaceSet()
{
    if (!m_fontFaceSet) {
        ASSERT(m_context);
        m_fontFaceSet = FontFaceSet::create(*m_context, m_cssFontFaceSet.get());
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

    m_paletteMap.clear();
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
    RefPtr<CSSValue> display = style.getPropertyCSSValue(CSSPropertyFontDisplay);
    if (!is<CSSValueList>(fontFamily) || !is<CSSValueList>(src) || (unicodeRange && !is<CSSValueList>(*unicodeRange)))
        return;

    CSSValueList& familyList = downcast<CSSValueList>(*fontFamily);
    if (!familyList.length())
        return;

    CSSValueList* rangeList = downcast<CSSValueList>(unicodeRange.get());

    CSSValueList& srcList = downcast<CSSValueList>(*src);
    if (!srcList.length())
        return;

    SetForScope creatingFont(m_creatingFont, true);
    auto fontFace = CSSFontFace::create(*this, &fontFaceRule);

    fontFace->setFamilies(familyList);
    if (fontStyle)
        fontFace->setStyle(*fontStyle);
    if (fontWeight)
        fontFace->setWeight(*fontWeight);
    if (fontStretch)
        fontFace->setStretch(*fontStretch);
    if (rangeList)
        fontFace->setUnicodeRange(*rangeList);
    if (featureSettings)
        fontFace->setFeatureSettings(*featureSettings);
    if (display)
        fontFace->setDisplay(downcast<CSSPrimitiveValue>(*display));

    CSSFontFace::appendSources(fontFace, srcList, m_context.get(), isInitiatingElementInUserAgentShadowTree);

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

void CSSFontSelector::addFontPaletteValuesRule(const StyleRuleFontPaletteValues& fontPaletteValuesRule)
{
    AtomString fontFamily = fontPaletteValuesRule.fontFamily().isNull() ? emptyAtom() : fontPaletteValuesRule.fontFamily();
    AtomString name = fontPaletteValuesRule.name().isNull() ? emptyAtom() : fontPaletteValuesRule.name();
    m_paletteMap.set(std::make_pair(fontFamily, name), fontPaletteValuesRule.fontPaletteValues());

    ++m_version;
}

void CSSFontSelector::addFontFeatureValuesRule(const StyleRuleFontFeatureValues& fontFeatureValuesRule)
{
    Ref<FontFeatureValues> fontFeatureValues = fontFeatureValuesRule.value();

    for (const auto& fontFamily : fontFeatureValuesRule.fontFamilies()) {
        auto exist = m_featureValues.get(fontFamily);
        if (exist)
            exist->updateOrInsert(fontFeatureValues.get());
        else
            m_featureValues.set(fontFamily, fontFeatureValues);
    }

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
        face->opportunisticallyStartFontDataURLLoading();
}

void CSSFontSelector::fontLoaded(CSSFontFace&)
{
    dispatchInvalidationCallbacks();
}

void CSSFontSelector::fontModified()
{
    if (!m_creatingFont && !m_buildIsUnderway)
        dispatchInvalidationCallbacks();
}

void CSSFontSelector::updateStyleIfNeeded()
{
    if (is<Document>(m_context))
        downcast<Document>(*m_context).updateStyleIfNeeded();
}

void CSSFontSelector::updateStyleIfNeeded(CSSFontFace&)
{
    updateStyleIfNeeded();
}

void CSSFontSelector::fontCacheInvalidated()
{
    dispatchInvalidationCallbacks();
}

std::optional<AtomString> CSSFontSelector::resolveGenericFamily(const FontDescription& fontDescription, const AtomString& familyName)
{
    auto platformResult = FontDescription::platformResolveGenericFamily(fontDescription.script(), fontDescription.computedLocale(), familyName);
    if (!platformResult.isNull())
        return platformResult;

    if (!m_context)
        return std::nullopt;

    const auto& settings = m_context->settingsValues();

    UScriptCode script = fontDescription.script();
    auto familyNameIndex = m_fontFamilyNames.find(familyName);
    if (familyNameIndex != notFound) {
        if (auto familyString = settings.fontGenericFamilies.fontFamily(static_cast<FamilyNamesIndex>(familyNameIndex), script))
            return AtomString(*familyString);
    }

    return std::nullopt;
}

const FontPaletteValues& CSSFontSelector::lookupFontPaletteValues(const AtomString& familyName, const FontDescription& fontDescription)
{
    static NeverDestroyed<FontPaletteValues> emptyFontPaletteValues;
    if (fontDescription.fontPalette().type != FontPalette::Type::Custom)
        return emptyFontPaletteValues.get();

    const AtomString paletteName = fontDescription.fontPalette().identifier;

    auto iterator = m_paletteMap.find(std::make_pair(familyName, paletteName));
    if (iterator == m_paletteMap.end())
        return emptyFontPaletteValues.get();
    return iterator->value;
}

RefPtr<FontFeatureValues> CSSFontSelector::lookupFontFeatureValues(const AtomString& familyName)
{
    auto iterator = m_featureValues.find(familyName);
    if (iterator == m_featureValues.end())
        return nullptr;

    return iterator->value.ptr();
}

FontRanges CSSFontSelector::fontRangesForFamily(const FontDescription& fontDescription, const AtomString& familyName)
{
    // If this ASSERT() fires, it usually means you forgot a document.updateStyleIfNeeded() somewhere.
    ASSERT(!m_buildIsUnderway || m_computingRootStyleFontCount);

    // FIXME: The spec (and Firefox) says user specified generic families (sans-serif etc.) should be resolved before the @font-face lookup too.
    bool resolveGenericFamilyFirst = familyName == m_fontFamilyNames.at(FamilyNamesIndex::StandardFamily);

    AtomString familyForLookup = familyName;
    std::optional<FontDescription> overrideFontDescription;
    const FontDescription* fontDescriptionForLookup = &fontDescription;
    auto resolveAndAssignGenericFamily = [&]() {
        if (auto genericFamilyOptional = resolveGenericFamily(fontDescription, familyName))
            familyForLookup = *genericFamilyOptional;
    };

    const auto& fontPaletteValues = lookupFontPaletteValues(familyName, fontDescription);
    auto fontFeatureValues = lookupFontFeatureValues(familyName);

    if (resolveGenericFamilyFirst)
        resolveAndAssignGenericFamily();
    auto* document = dynamicDowncast<Document>(m_context.get());
    auto* face = m_cssFontFaceSet->fontFace(fontDescriptionForLookup->fontSelectionRequest(), familyForLookup);
    if (face) {
        if (document && DeprecatedGlobalSettings::webAPIStatisticsEnabled())
            ResourceLoadObserver::shared().logFontLoad(*document, familyForLookup.string(), true);
        return face->fontRanges(*fontDescriptionForLookup, fontPaletteValues, fontFeatureValues);
    }

    if (!resolveGenericFamilyFirst)
        resolveAndAssignGenericFamily();

    auto font = FontCache::forCurrentThread().fontForFamily(*fontDescriptionForLookup, familyForLookup, { { }, { }, fontPaletteValues, fontFeatureValues });
    if (document && DeprecatedGlobalSettings::webAPIStatisticsEnabled())
        ResourceLoadObserver::shared().logFontLoad(*document, familyForLookup.string(), !!font);
    return FontRanges { WTFMove(font) };
}

void CSSFontSelector::clearFonts()
{
    m_isStopped = true;
    m_cssFontFaceSet->clear();
    m_clients.clear();
}

size_t CSSFontSelector::fallbackFontCount()
{
    if (m_isStopped)
        return 0;

    return m_context->settingsValues().fontFallbackPrefersPictographs ? 1 : 0;
}

RefPtr<Font> CSSFontSelector::fallbackFontAt(const FontDescription& fontDescription, size_t index)
{
    ASSERT_UNUSED(index, !index);

    if (m_isStopped)
        return nullptr;

    if (!m_context->settingsValues().fontFallbackPrefersPictographs)
        return nullptr;
    auto& pictographFontFamily = m_context->settingsValues().fontGenericFamilies.pictographFontFamily();
    auto font = FontCache::forCurrentThread().fontForFamily(fontDescription, pictographFontFamily);
    if (DeprecatedGlobalSettings::webAPIStatisticsEnabled() && is<Document>(m_context))
        ResourceLoadObserver::shared().logFontLoad(downcast<Document>(*m_context), pictographFontFamily, !!font);

    return font;
}

}
