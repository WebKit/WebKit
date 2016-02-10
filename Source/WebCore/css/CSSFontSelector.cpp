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
#include "CSSFontFaceRule.h"
#include "CSSFontFaceSource.h"
#include "CSSFontFaceSrcValue.h"
#include "CSSFontFamily.h"
#include "CSSFontFeatureValue.h"
#include "CSSPrimitiveValue.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSPropertyNames.h"
#include "CSSSegmentedFontFace.h"
#include "CSSUnicodeRangeValue.h"
#include "CSSValueKeywords.h"
#include "CSSValueList.h"
#include "CachedResourceLoader.h"
#include "Document.h"
#include "Font.h"
#include "FontCache.h"
#include "FontVariantBuilder.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "SVGFontFaceElement.h"
#include "SVGNames.h"
#include "Settings.h"
#include "StyleProperties.h"
#include "StyleResolver.h"
#include "StyleRule.h"
#include "WebKitFontFamilyNames.h"
#include <wtf/Ref.h>
#include <wtf/text/AtomicString.h>

namespace WebCore {

static unsigned fontSelectorId;

CSSFontSelector::CSSFontSelector(Document& document)
    : m_document(&document)
    , m_beginLoadingTimer(*this, &CSSFontSelector::beginLoadTimerFired)
    , m_uniqueId(++fontSelectorId)
    , m_version(0)
{
    // FIXME: An old comment used to say there was no need to hold a reference to m_document
    // because "we are guaranteed to be destroyed before the document". But there does not
    // seem to be any such guarantee.

    ASSERT(m_document);
    FontCache::singleton().addClient(*this);
}

CSSFontSelector::~CSSFontSelector()
{
    clearDocument();
    FontCache::singleton().removeClient(*this);
}

bool CSSFontSelector::isEmpty() const
{
    return m_fonts.isEmpty();
}

static Optional<FontTraitsMask> computeTraitsMask(const StyleProperties& style)
{
    unsigned traitsMask = 0;

    if (RefPtr<CSSValue> fontStyle = style.getPropertyCSSValue(CSSPropertyFontStyle)) {
        if (!is<CSSPrimitiveValue>(*fontStyle))
            return Nullopt;

        switch (downcast<CSSPrimitiveValue>(*fontStyle).getValueID()) {
        case CSSValueNormal:
            traitsMask |= FontStyleNormalMask;
            break;
        case CSSValueItalic:
        case CSSValueOblique:
            traitsMask |= FontStyleItalicMask;
            break;
        default:
            break;
        }
    } else
        traitsMask |= FontStyleNormalMask;

    if (RefPtr<CSSValue> fontWeight = style.getPropertyCSSValue(CSSPropertyFontWeight)) {
        if (!is<CSSPrimitiveValue>(*fontWeight))
            return Nullopt;

        switch (downcast<CSSPrimitiveValue>(*fontWeight).getValueID()) {
        case CSSValueBold:
        case CSSValue700:
            traitsMask |= FontWeight700Mask;
            break;
        case CSSValueNormal:
        case CSSValue400:
            traitsMask |= FontWeight400Mask;
            break;
        case CSSValue900:
            traitsMask |= FontWeight900Mask;
            break;
        case CSSValue800:
            traitsMask |= FontWeight800Mask;
            break;
        case CSSValue600:
            traitsMask |= FontWeight600Mask;
            break;
        case CSSValue500:
            traitsMask |= FontWeight500Mask;
            break;
        case CSSValue300:
            traitsMask |= FontWeight300Mask;
            break;
        case CSSValue200:
            traitsMask |= FontWeight200Mask;
            break;
        case CSSValue100:
            traitsMask |= FontWeight100Mask;
            break;
        default:
            break;
        }
    } else
        traitsMask |= FontWeight400Mask;

    return static_cast<FontTraitsMask>(traitsMask);
}

static Ref<CSSFontFace> createFontFace(CSSValueList& srcList, FontTraitsMask traitsMask, Document* document, bool isInitiatingElementInUserAgentShadowTree)
{
    Ref<CSSFontFace> fontFace = CSSFontFace::create(traitsMask);

    for (auto& src : srcList) {
        // An item in the list either specifies a string (local font name) or a URL (remote font to download).
        CSSFontFaceSrcValue& item = downcast<CSSFontFaceSrcValue>(src.get());
        std::unique_ptr<CSSFontFaceSource> source;
        SVGFontFaceElement* fontFaceElement = nullptr;
        bool foundSVGFont = false;

#if ENABLE(SVG_FONTS)
        foundSVGFont = item.isSVGFontFaceSrc() || item.svgFontFaceElement();
        fontFaceElement = item.svgFontFaceElement();
#endif
        if (!item.isLocal()) {
            Settings* settings = document ? document->settings() : nullptr;
            bool allowDownloading = foundSVGFont || (settings && settings->downloadableBinaryFontsEnabled());
            if (allowDownloading && item.isSupportedFormat() && document) {
                if (CachedFont* cachedFont = item.cachedFont(document, foundSVGFont, isInitiatingElementInUserAgentShadowTree))
                    source = std::make_unique<CSSFontFaceSource>(fontFace.get(), item.resource(), cachedFont);
            }
        } else
            source = std::make_unique<CSSFontFaceSource>(fontFace.get(), item.resource(), nullptr, fontFaceElement);

        if (source)
            fontFace->adoptSource(WTFMove(source));
    }

    return fontFace;
}

static String familyNameFromPrimitive(const CSSPrimitiveValue& value)
{
    if (value.isFontFamily())
        return value.fontFamily().familyName;
    if (!value.isValueID())
        return { };

    // We need to use the raw text for all the generic family types, since @font-face is a way of actually
    // defining what font to use for those types.
    switch (value.getValueID()) {
    case CSSValueSerif:
        return serifFamily;
    case CSSValueSansSerif:
        return sansSerifFamily;
    case CSSValueCursive:
        return cursiveFamily;
    case CSSValueFantasy:
        return fantasyFamily;
    case CSSValueMonospace:
        return monospaceFamily;
    case CSSValueWebkitPictograph:
        return pictographFamily;
    default:
        return { };
    }
}

static void registerLocalFontFacesForFamily(const String& familyName, HashMap<String, Vector<Ref<CSSFontFace>>, ASCIICaseInsensitiveHash>& locallyInstalledFontFaces)
{
    ASSERT(!locallyInstalledFontFaces.contains(familyName));

    Vector<FontTraitsMask> traitsMasks = FontCache::singleton().getTraitsInFamily(familyName);
    if (traitsMasks.isEmpty())
        return;

    Vector<Ref<CSSFontFace>> faces = { };
    for (auto mask : traitsMasks) {
        Ref<CSSFontFace> face = CSSFontFace::create(mask, true);
        face->adoptSource(std::make_unique<CSSFontFaceSource>(face.get(), familyName));
        ASSERT(!face->allSourcesFailed());
        faces.append(WTFMove(face));
    }
    locallyInstalledFontFaces.add(familyName, WTFMove(faces));
}

void CSSFontSelector::addFontFaceRule(const StyleRuleFontFace& fontFaceRule, bool isInitiatingElementInUserAgentShadowTree)
{
    const StyleProperties& style = fontFaceRule.properties();
    RefPtr<CSSValue> fontFamily = style.getPropertyCSSValue(CSSPropertyFontFamily);
    RefPtr<CSSValue> src = style.getPropertyCSSValue(CSSPropertySrc);
    RefPtr<CSSValue> unicodeRange = style.getPropertyCSSValue(CSSPropertyUnicodeRange);
    RefPtr<CSSValue> featureSettings = style.getPropertyCSSValue(CSSPropertyFontFeatureSettings);
    RefPtr<CSSValue> variantLigatures = style.getPropertyCSSValue(CSSPropertyFontVariantLigatures);
    RefPtr<CSSValue> variantPosition = style.getPropertyCSSValue(CSSPropertyFontVariantPosition);
    RefPtr<CSSValue> variantCaps = style.getPropertyCSSValue(CSSPropertyFontVariantCaps);
    RefPtr<CSSValue> variantNumeric = style.getPropertyCSSValue(CSSPropertyFontVariantNumeric);
    RefPtr<CSSValue> variantAlternates = style.getPropertyCSSValue(CSSPropertyFontVariantAlternates);
    RefPtr<CSSValue> variantEastAsian = style.getPropertyCSSValue(CSSPropertyFontVariantEastAsian);
    if (!is<CSSValueList>(fontFamily.get()) || !is<CSSValueList>(src.get()) || (unicodeRange && !is<CSSValueList>(*unicodeRange)))
        return;

    CSSValueList& familyList = downcast<CSSValueList>(*fontFamily);
    if (!familyList.length())
        return;

    CSSValueList& srcList = downcast<CSSValueList>(*src);
    if (!srcList.length())
        return;

    CSSValueList* rangeList = downcast<CSSValueList>(unicodeRange.get());

    auto computedTraitsMask = computeTraitsMask(style);
    if (!computedTraitsMask)
        return;
    auto traitsMask = computedTraitsMask.value();

    Ref<CSSFontFace> fontFace = createFontFace(srcList, traitsMask, m_document, isInitiatingElementInUserAgentShadowTree);
    if (fontFace->allSourcesFailed())
        return;

    if (rangeList) {
        unsigned numRanges = rangeList->length();
        for (unsigned i = 0; i < numRanges; i++) {
            CSSUnicodeRangeValue& range = downcast<CSSUnicodeRangeValue>(*rangeList->itemWithoutBoundsCheck(i));
            fontFace->addRange(range.from(), range.to());
        }
    }

    if (featureSettings) {
        for (auto& item : downcast<CSSValueList>(*featureSettings)) {
            auto& feature = downcast<CSSFontFeatureValue>(item.get());
            fontFace->insertFeature(FontFeature(feature.tag(), feature.value()));
        }
    }

    if (variantLigatures)
        applyValueFontVariantLigatures(fontFace.get(), *variantLigatures);

    if (variantPosition && is<CSSPrimitiveValue>(*variantPosition))
        fontFace->setVariantPosition(downcast<CSSPrimitiveValue>(*variantPosition));

    if (variantCaps && is<CSSPrimitiveValue>(*variantCaps))
        fontFace->setVariantCaps(downcast<CSSPrimitiveValue>(*variantCaps));

    if (variantNumeric)
        applyValueFontVariantNumeric(fontFace.get(), *variantNumeric);

    if (variantAlternates && is<CSSPrimitiveValue>(*variantAlternates))
        fontFace->setVariantAlternates(downcast<CSSPrimitiveValue>(*variantAlternates));

    if (variantEastAsian)
        applyValueFontVariantEastAsian(fontFace.get(), *variantEastAsian);

    for (auto& item : familyList) {
        String familyName = familyNameFromPrimitive(downcast<CSSPrimitiveValue>(item.get()));
        if (familyName.isEmpty())
            continue;

        auto addResult = m_fontFaces.add(familyName, Vector<Ref<CSSFontFace>>());
        auto& familyFontFaces = addResult.iterator->value;
        if (addResult.isNewEntry) {
            registerLocalFontFacesForFamily(familyName, m_locallyInstalledFontFaces);
            familyFontFaces = { };
        }

        familyFontFaces.append(fontFace.copyRef());
        
        ++m_version;
    }
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

    Vector<FontSelectorClient*> clients;
    copyToVector(m_clients, clients);
    for (size_t i = 0; i < clients.size(); ++i)
        clients[i]->fontsNeedUpdate(*this);
}

void CSSFontSelector::fontLoaded()
{
    dispatchInvalidationCallbacks();
}

void CSSFontSelector::fontCacheInvalidated()
{
    dispatchInvalidationCallbacks();
}

static const AtomicString& resolveGenericFamily(Document* document, const FontDescription& fontDescription, const AtomicString& familyName)
{
    if (!document || !document->frame())
        return familyName;

    const Settings& settings = document->frame()->settings();

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

    return familyName;
}

class FontFaceComparator {
public:
    FontFaceComparator(FontTraitsMask desiredTraitsMaskForComparison)
        : m_desiredTraitsMaskForComparison(desiredTraitsMaskForComparison)
    {
        ASSERT_WITH_SECURITY_IMPLICATION(m_desiredTraitsMaskForComparison & FontWeightMask);
    }

    bool operator()(const CSSFontFace& first, const CSSFontFace& second)
    {
        FontTraitsMask firstTraitsMask = first.traitsMask();
        FontTraitsMask secondTraitsMask = second.traitsMask();

        bool firstHasDesiredStyle = firstTraitsMask & m_desiredTraitsMaskForComparison & FontStyleMask;
        bool secondHasDesiredStyle = secondTraitsMask & m_desiredTraitsMaskForComparison & FontStyleMask;

        if (firstHasDesiredStyle != secondHasDesiredStyle)
            return firstHasDesiredStyle;

        if ((m_desiredTraitsMaskForComparison & FontStyleItalicMask) && !first.isLocalFallback() && !second.isLocalFallback()) {
            // Prefer a font that has indicated that it can only support italics to a font that claims to support
            // all styles. The specialized font is more likely to be the one the author wants used.
            bool firstRequiresItalics = (firstTraitsMask & FontStyleItalicMask) && !(firstTraitsMask & FontStyleNormalMask);
            bool secondRequiresItalics = (secondTraitsMask & FontStyleItalicMask) && !(secondTraitsMask & FontStyleNormalMask);
            if (firstRequiresItalics != secondRequiresItalics)
                return firstRequiresItalics;
        }

        if (secondTraitsMask & m_desiredTraitsMaskForComparison & FontWeightMask)
            return false;
        if (firstTraitsMask & m_desiredTraitsMaskForComparison & FontWeightMask)
            return true;

        // http://www.w3.org/TR/2011/WD-css3-fonts-20111004/#font-matching-algorithm says :
        //   - If the desired weight is less than 400, weights below the desired weight are checked in descending order followed by weights above the desired weight in ascending order until a match is found.
        //   - If the desired weight is greater than 500, weights above the desired weight are checked in ascending order followed by weights below the desired weight in descending order until a match is found.
        //   - If the desired weight is 400, 500 is checked first and then the rule for desired weights less than 400 is used.
        //   - If the desired weight is 500, 400 is checked first and then the rule for desired weights less than 400 is used.

        static const unsigned fallbackRuleSets = 9;
        static const unsigned rulesPerSet = 8;
        static const FontTraitsMask weightFallbackRuleSets[fallbackRuleSets][rulesPerSet] = {
            { FontWeight200Mask, FontWeight300Mask, FontWeight400Mask, FontWeight500Mask, FontWeight600Mask, FontWeight700Mask, FontWeight800Mask, FontWeight900Mask },
            { FontWeight100Mask, FontWeight300Mask, FontWeight400Mask, FontWeight500Mask, FontWeight600Mask, FontWeight700Mask, FontWeight800Mask, FontWeight900Mask },
            { FontWeight200Mask, FontWeight100Mask, FontWeight400Mask, FontWeight500Mask, FontWeight600Mask, FontWeight700Mask, FontWeight800Mask, FontWeight900Mask },
            { FontWeight500Mask, FontWeight300Mask, FontWeight200Mask, FontWeight100Mask, FontWeight600Mask, FontWeight700Mask, FontWeight800Mask, FontWeight900Mask },
            { FontWeight400Mask, FontWeight300Mask, FontWeight200Mask, FontWeight100Mask, FontWeight600Mask, FontWeight700Mask, FontWeight800Mask, FontWeight900Mask },
            { FontWeight700Mask, FontWeight800Mask, FontWeight900Mask, FontWeight500Mask, FontWeight400Mask, FontWeight300Mask, FontWeight200Mask, FontWeight100Mask },
            { FontWeight800Mask, FontWeight900Mask, FontWeight600Mask, FontWeight500Mask, FontWeight400Mask, FontWeight300Mask, FontWeight200Mask, FontWeight100Mask },
            { FontWeight900Mask, FontWeight700Mask, FontWeight600Mask, FontWeight500Mask, FontWeight400Mask, FontWeight300Mask, FontWeight200Mask, FontWeight100Mask },
            { FontWeight800Mask, FontWeight700Mask, FontWeight600Mask, FontWeight500Mask, FontWeight400Mask, FontWeight300Mask, FontWeight200Mask, FontWeight100Mask }
        };

        unsigned ruleSetIndex = 0;
        for (; !(m_desiredTraitsMaskForComparison & (1 << (FontWeight100Bit + ruleSetIndex))); ruleSetIndex++) { }

        const FontTraitsMask* weightFallbackRule = weightFallbackRuleSets[ruleSetIndex];
        for (unsigned i = 0; i < rulesPerSet; ++i) {
            if (secondTraitsMask & weightFallbackRule[i])
                return false;
            if (firstTraitsMask & weightFallbackRule[i])
                return true;
        }

        return false;
    }

private:
    FontTraitsMask m_desiredTraitsMaskForComparison;
};

FontRanges CSSFontSelector::fontRangesForFamily(const FontDescription& fontDescription, const AtomicString& familyName)
{
    // FIXME: The spec (and Firefox) says user specified generic families (sans-serif etc.) should be resolved before the @font-face lookup too.
    bool resolveGenericFamilyFirst = familyName == standardFamily;

    AtomicString familyForLookup = resolveGenericFamilyFirst ? resolveGenericFamily(m_document, fontDescription, familyName) : familyName;
    CSSSegmentedFontFace* face = getFontFace(fontDescription, familyForLookup);
    if (!face) {
        if (!resolveGenericFamilyFirst)
            familyForLookup = resolveGenericFamily(m_document, fontDescription, familyName);
        return FontRanges(FontCache::singleton().fontForFamily(fontDescription, familyForLookup));
    }

    return face->fontRanges(fontDescription);
}

CSSSegmentedFontFace* CSSFontSelector::getFontFace(const FontDescription& fontDescription, const AtomicString& family)
{
    auto iterator = m_fontFaces.find(family);
    if (iterator == m_fontFaces.end())
        return nullptr;
    auto& familyFontFaces = iterator->value;

    auto& segmentedFontFaceCache = m_fonts.add(family, HashMap<unsigned, std::unique_ptr<CSSSegmentedFontFace>>()).iterator->value;

    FontTraitsMask traitsMask = fontDescription.traitsMask();

    std::unique_ptr<CSSSegmentedFontFace>& face = segmentedFontFaceCache.add(traitsMask, nullptr).iterator->value;
    if (face)
        return face.get();

    face = std::make_unique<CSSSegmentedFontFace>(*this);

    Vector<std::reference_wrapper<CSSFontFace>, 32> candidateFontFaces;
    for (int i = familyFontFaces.size() - 1; i >= 0; --i) {
        CSSFontFace& candidate = familyFontFaces[i];
        unsigned candidateTraitsMask = candidate.traitsMask();
        if ((traitsMask & FontStyleNormalMask) && !(candidateTraitsMask & FontStyleNormalMask))
            continue;
        candidateFontFaces.append(candidate);
    }

    auto localIterator = m_locallyInstalledFontFaces.find(family);
    if (localIterator != m_locallyInstalledFontFaces.end()) {
        for (auto& candidate : localIterator->value) {
            unsigned candidateTraitsMask = candidate->traitsMask();
            if ((traitsMask & FontStyleNormalMask) && !(candidateTraitsMask & FontStyleNormalMask))
                continue;
            candidateFontFaces.append(candidate);
        }
    }

    std::stable_sort(candidateFontFaces.begin(), candidateFontFaces.end(), FontFaceComparator(traitsMask));
    for (auto& candidate : candidateFontFaces)
        face->appendFontFace(candidate.get());

    return face.get();
}

void CSSFontSelector::clearDocument()
{
    if (!m_document) {
        ASSERT(!m_beginLoadingTimer.isActive());
        ASSERT(m_fontsToBeginLoading.isEmpty());
        return;
    }

    m_beginLoadingTimer.stop();

    CachedResourceLoader& cachedResourceLoader = m_document->cachedResourceLoader();
    for (auto& fontHandle : m_fontsToBeginLoading) {
        // Balances incrementRequestCount() in beginLoadingFontSoon().
        cachedResourceLoader.decrementRequestCount(fontHandle.get());
    }
    m_fontsToBeginLoading.clear();

    m_document = nullptr;
}

void CSSFontSelector::beginLoadingFontSoon(CachedFont* font)
{
    if (!m_document)
        return;

    m_fontsToBeginLoading.append(font);
    // Increment the request count now, in order to prevent didFinishLoad from being dispatched
    // after this font has been requested but before it began loading. Balanced by
    // decrementRequestCount() in beginLoadTimerFired() and in clearDocument().
    m_document->cachedResourceLoader().incrementRequestCount(font);
    m_beginLoadingTimer.startOneShot(0);
}

void CSSFontSelector::beginLoadTimerFired()
{
    Vector<CachedResourceHandle<CachedFont>> fontsToBeginLoading;
    fontsToBeginLoading.swap(m_fontsToBeginLoading);

    // CSSFontSelector could get deleted via beginLoadIfNeeded() or loadDone() unless protected.
    Ref<CSSFontSelector> protect(*this);

    CachedResourceLoader& cachedResourceLoader = m_document->cachedResourceLoader();
    for (auto& fontHandle : fontsToBeginLoading) {
        fontHandle->beginLoadIfNeeded(cachedResourceLoader);
        // Balances incrementRequestCount() in beginLoadingFontSoon().
        cachedResourceLoader.decrementRequestCount(fontHandle.get());
    }
    // Ensure that if the request count reaches zero, the frame loader will know about it.
    cachedResourceLoader.loadDone(nullptr);
    // New font loads may be triggered by layout after the document load is complete but before we have dispatched
    // didFinishLoading for the frame. Make sure the delegate is always dispatched by checking explicitly.
    if (m_document && m_document->frame())
        m_document->frame()->loader().checkLoadComplete();
}

size_t CSSFontSelector::fallbackFontCount()
{
    if (!m_document)
        return 0;

    if (Settings* settings = m_document->settings())
        return settings->fontFallbackPrefersPictographs() ? 1 : 0;

    return 0;
}

RefPtr<Font> CSSFontSelector::fallbackFontAt(const FontDescription& fontDescription, size_t index)
{
    ASSERT_UNUSED(index, !index);

    if (!m_document)
        return nullptr;

    Settings* settings = m_document->settings();
    if (!settings || !settings->fontFallbackPrefersPictographs())
        return nullptr;

    return FontCache::singleton().fontForFamily(fontDescription, settings->pictographFontFamily());
}

}
