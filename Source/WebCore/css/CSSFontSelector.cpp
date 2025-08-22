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

#include "CSSCounterStyleRegistry.h"
#include "CSSFontFace.h"
#include "CSSFontFaceSource.h"
#include "CSSFontFeatureValuesRule.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyNames.h"
#include "CSSSegmentedFontFace.h"
#include "CSSValueKeywords.h"
#include "CSSValueList.h"
#include "CachedFont.h"
#include "CachedResourceLoader.h"
#include "Document.h"
#include "DocumentInlines.h"
#include "Font.h"
#include "FontCache.h"
#include "FontCascadeDescription.h"
#include "FontFace.h"
#include "FontFaceSet.h"
#include "FontSelectorClient.h"
#include "FrameLoader.h"
#include "LocalFrame.h"
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
            m_fontFamilyNames.constructAndAppend(familyName);
    } else {
        m_fontFamilyNames.appendContainerWithMapping(familyNamesData.get(), [](auto& familyName) {
            return familyName;
        });
    }

    FontCache::forCurrentThread()->addClient(*this);
    m_cssFontFaceSet->addFontModifiedObserver(m_fontModifiedObserver);
    LOG(Fonts, "CSSFontSelector %p ctor", this);
}

CSSFontSelector::~CSSFontSelector()
{
    LOG(Fonts, "CSSFontSelector %p dtor", this);

    clearFonts();

    if (CheckedPtr fontCache = FontCache::forCurrentThreadIfNotDestroyed())
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
        m_fontFaceSet = FontFaceSet::create(protectedScriptExecutionContext(), m_cssFontFaceSet.get());
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
        Ref face = m_cssFontFaceSet.get()[i];
        if (face->cssConnection())
            m_cssConnectionsPossiblyToRemove.add(face.get());
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
        RefPtr connection = face->cssConnection();
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

    Ref style = fontFaceRule.properties();
    RefPtr fontFamily = style->getPropertyCSSValue(CSSPropertyFontFamily);
    RefPtr fontStyle = style->getPropertyCSSValue(CSSPropertyFontStyle);
    RefPtr fontWeight = style->getPropertyCSSValue(CSSPropertyFontWeight);
    RefPtr fontWidth = style->getPropertyCSSValue(CSSPropertyFontWidth);
    RefPtr srcList = dynamicDowncast<CSSValueList>(style->getPropertyCSSValue(CSSPropertySrc));
    RefPtr unicodeRange = style->getPropertyCSSValue(CSSPropertyUnicodeRange);
    RefPtr rangeList = downcast<CSSValueList>(unicodeRange.get());
    RefPtr featureSettings = style->getPropertyCSSValue(CSSPropertyFontFeatureSettings);
    RefPtr display = style->getPropertyCSSValue(CSSPropertyFontDisplay);
    RefPtr sizeAdjust = style->getPropertyCSSValue(CSSPropertySizeAdjust);
    if (!fontFamily || !srcList || (unicodeRange && !rangeList))
        return;

    if (!srcList->length())
        return;

    SetForScope creatingFont(m_creatingFont, true);
    auto fontFace = CSSFontFace::create(*this, &fontFaceRule);

    fontFace->setFamily(*fontFamily);
    if (fontStyle)
        fontFace->setStyle(*fontStyle);
    if (fontWeight)
        fontFace->setWeight(*fontWeight);
    if (fontWidth)
        fontFace->setWidth(*fontWidth);
    if (rangeList)
        fontFace->setUnicodeRange(*rangeList);
    if (featureSettings)
        fontFace->setFeatureSettings(*featureSettings);
    if (display)
        fontFace->setDisplay(downcast<CSSPrimitiveValue>(*display));
    if (sizeAdjust)
        fontFace->setSizeAdjust(*sizeAdjust);

    CSSFontFace::appendSources(fontFace, *srcList, protectedScriptExecutionContext().ptr(), isInitiatingElementInUserAgentShadowTree);

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
        if (RefPtr existingWrapper = existingFace->existingWrapper())
            existingWrapper->adopt(fontFace.get());
    }

    m_cssFontFaceSet->add(fontFace.get());
    ++m_version;
}

void CSSFontSelector::addFontPaletteValuesRule(const StyleRuleFontPaletteValues& fontPaletteValuesRule)
{

    auto& name = fontPaletteValuesRule.name();
    ASSERT(!name.isNull());

    auto& fontFamilies = fontPaletteValuesRule.fontFamilies();
    if (fontFamilies.isEmpty())
        return;

    for (auto& fontFamily : fontFamilies)
        m_paletteMap.set(std::make_pair(fontFamily, name), fontPaletteValuesRule.fontPaletteValues());

    ++m_version;
}

void CSSFontSelector::addFontFeatureValuesRule(const StyleRuleFontFeatureValues& fontFeatureValuesRule)
{
    Ref<FontFeatureValues> fontFeatureValues = fontFeatureValuesRule.value();

    for (const auto& fontFamily : fontFeatureValuesRule.fontFamilies()) {
        // https://www.w3.org/TR/css-fonts-3/#font-family-casing
        auto lowercased = fontFamily.string().convertToLowercaseWithoutLocale();
        if (RefPtr exist = m_featureValues.get(lowercased))
            exist->updateOrInsert(fontFeatureValues.get());
        else
            m_featureValues.set(lowercased, fontFeatureValues);
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

    for (auto& client : copyToVector(m_clients)) {
        if (m_clients.contains(client))
            client->fontsNeedUpdate(*this);
    }
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
    if (RefPtr document = dynamicDowncast<Document>(m_context.get()))
        document->updateStyleIfNeeded();
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

    const auto& settings = protectedScriptExecutionContext()->settingsValues();

    UScriptCode script = fontDescription.script();
    auto familyNameIndex = m_fontFamilyNames.find(familyName);
    if (familyNameIndex != notFound) {
        if (auto familyString = settings.fontGenericFamilies.fontFamily(static_cast<FamilyNamesIndex>(familyNameIndex), script))
            return AtomString(*familyString);
    }

    return std::nullopt;
}

const FontPaletteValues& CSSFontSelector::lookupFontPaletteValues(const AtomString& familyName, const FontDescription& fontDescription) const
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

RefPtr<FontFeatureValues> CSSFontSelector::lookupFontFeatureValues(const AtomString& familyName) const
{
    // https://www.w3.org/TR/css-fonts-3/#font-family-casing
    auto lowercased = familyName.string().convertToLowercaseWithoutLocale();
    auto iterator = m_featureValues.find(lowercased);
    if (iterator == m_featureValues.end())
        return nullptr;

    return iterator->value.ptr();
}

// Fonts with appropriate Unicode coverage and OpenType features are required for good math
// rendering. These requirements as well as the up-to-date list of known math fonts to fulfill
// these requirements are listed on http://trac.webkit.org/wiki/MathML/Fonts.
using MathFontList = std::array<AtomString, 19>;
static const MathFontList& mathFontList()
{
    static const NeverDestroyed list = MathFontList {
        // This font has Computer Modern style and is provided with most TeX & Linux distributions.
        // We put it as the default because its style is familiar to TeX, Wikipedia and math people.
        "Latin Modern Math"_s,
        // The following fonts have Times style and are provided with most TeX & Linux distributions.
        // We put XITS & STIX as a second option because they have very good unicode coverage.
        // STIX Two is a complete redesign of STIX that fixes serious bugs in version one so we put it in first position.
        // XITS is a fork of STIX with bug fixes and more Arabic/RTL features so we put it in second position.
        "STIX Two Math"_s,
        "XITS Math"_s,
        "STIX Math"_s,
        "Libertinus Math"_s,
        "TeX Gyre Termes Math"_s,
        // These fonts respectively have style compatible with Bookman Old and Century Schoolbook.
        // They are provided with most TeX & Linux distributions.
        "TeX Gyre Bonum Math"_s,
        "TeX Gyre Schola"_s,
        // DejaVu is pre-installed on many Linux distributions and is included in LibreOffice.
        "DejaVu Math TeX Gyre"_s,
        // The following fonts have Palatino style and are provided with most TeX & Linux distributions.
        // Asana Math has some rendering issues (e.g. missing italic correction) so we put it after.
        "TeX Gyre Pagella Math"_s,
        "Asana Math"_s,
        // The following fonts are proprietary and have not much been tested so we put them at the end.
        // Cambria Math it is pre-installed on Windows 7 and higher.
        "Cambria Math"_s,
        "Lucida Bright Math"_s,
        "Minion Math"_s,
        // The following fonts do not satisfy the requirements for good mathematical rendering.
        // These are pre-installed on Mac and iOS so we list them to provide minimal unicode-based
        // mathematical rendering. For more explanation of fallback mechanisms and missing features see
        // http://trac.webkit.org/wiki/MathML/Fonts#ObsoleteFontsandFallbackMechanisms.
        // STIX fonts have best unicode coverage so we put them first.
        "STIXGeneral"_s,
        "STIXSizeOneSym"_s,
        "Symbol"_s,
        "Times New Roman"_s,
        // Mathematical fonts generally use "serif" style. Hence we append the generic "serif" family
        // as a fallback in order to increase our chance to find a mathematical font.
        "serif"_s,
    };
    return list;
}

FontRanges CSSFontSelector::fontRangesForFamily(const FontDescription& fontDescription, const AtomString& familyName)
{
    // If this ASSERT() fires, it usually means you forgot a document.updateStyleIfNeeded() somewhere.
    ASSERT(!m_buildIsUnderway || m_computingRootStyleFontCount);

    // FIXME: The spec (and Firefox) says user specified generic families (sans-serif etc.) should be resolved before the @font-face lookup too.
    bool resolveGenericFamilyFirst = familyName == m_fontFamilyNames.at(FamilyNamesIndex::StandardFamily);

    AtomString familyForLookup = familyName;
    auto isGenericFontFamily = IsGenericFontFamily::No;
    const FontDescription* fontDescriptionForLookup = &fontDescription;
    auto resolveAndAssignGenericFamily = [&] {
        if (auto genericFamilyOptional = resolveGenericFamily(fontDescription, familyName)) {
            familyForLookup = *genericFamilyOptional;
            isGenericFontFamily = IsGenericFontFamily::Yes;
        }
    };

    const auto& fontPaletteValues = lookupFontPaletteValues(familyName, fontDescription);
    auto fontFeatureValues = lookupFontFeatureValues(familyName);

    // Handle the generic math font family a bit differently.
    if (familyName == m_fontFamilyNames.at(FamilyNamesIndex::MathFamily)) {
        // TODO: Check if the user has defined a preference (http://webkit.org/b/156843).
        // Iterate through the font list to find a valid fallback.
        for (auto& family : mathFontList()) {
            auto ranges = fontRangesForFamily(fontDescription, family);
            if (!ranges.isNull())
                return FontRanges(WTFMove(ranges), IsGenericFontFamily::Yes);
        }
    }

    if (resolveGenericFamilyFirst)
        resolveAndAssignGenericFamily();
    RefPtr document = dynamicDowncast<Document>(m_context.get());
    if (RefPtr face = m_cssFontFaceSet->fontFace(fontDescriptionForLookup->fontSelectionRequest(), familyForLookup)) {
        if (document && document->settings().webAPIStatisticsEnabled())
            ResourceLoadObserver::shared().logFontLoad(*document, familyForLookup.string(), true);
        return { face->fontRanges(*fontDescriptionForLookup, fontPaletteValues, fontFeatureValues), isGenericFontFamily };
    }

    if (!resolveGenericFamilyFirst)
        resolveAndAssignGenericFamily();

    auto font = FontCache::forCurrentThread()->fontForFamily(*fontDescriptionForLookup, familyForLookup, { { }, { }, fontPaletteValues, fontFeatureValues, 1.0 });
    if (document && document->settings().webAPIStatisticsEnabled())
        ResourceLoadObserver::shared().logFontLoad(*document, familyForLookup.string(), !!font);
    return { FontRanges { WTFMove(font) }, isGenericFontFamily };
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

    return protectedScriptExecutionContext()->settingsValues().fontFallbackPrefersPictographs ? 1 : 0;
}

RefPtr<Font> CSSFontSelector::fallbackFontAt(const FontDescription& fontDescription, size_t index)
{
    ASSERT_UNUSED(index, !index);

    if (m_isStopped)
        return nullptr;

    RefPtr context = m_context.get();
    if (!context->settingsValues().fontFallbackPrefersPictographs)
        return nullptr;
    auto& pictographFontFamily = context->settingsValues().fontGenericFamilies.pictographFontFamily();
    RefPtr font = FontCache::forCurrentThread()->fontForFamily(fontDescription, pictographFontFamily);
    if (RefPtr document = dynamicDowncast<Document>(context.get()); document && document->settingsValues().webAPIStatisticsEnabled)
        ResourceLoadObserver::shared().logFontLoad(*document, pictographFontFamily, !!font);

    return font;
}

bool CSSFontSelector::isSimpleFontSelectorForDescription() const
{
    // font face rules still pending
    if (m_stagingArea.size())
        return false;

    // FIXME: remove this when we fix counter style rules mutation.
    if (RefPtr document = dynamicDowncast<Document>(m_context.get())) {
        if (document->counterStyleRegistry().hasAuthorCounterStyles())
            return false;
    }

    return !m_cssFontFaceSet->faceCount() && m_featureValues.isEmpty() && m_paletteMap.isEmpty();
}

}
