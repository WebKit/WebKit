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
    FontCache::singleton().addClient(this);
}

CSSFontSelector::~CSSFontSelector()
{
    clearDocument();
    FontCache::singleton().removeClient(this);
}

bool CSSFontSelector::isEmpty() const
{
    return m_fonts.isEmpty();
}

void CSSFontSelector::addFontFaceRule(const StyleRuleFontFace* fontFaceRule, bool isInitiatingElementInUserAgentShadowTree)
{
    // Obtain the font-family property and the src property.  Both must be defined.
    const StyleProperties& style = fontFaceRule->properties();
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

    unsigned traitsMask = 0;

    if (RefPtr<CSSValue> fontStyle = style.getPropertyCSSValue(CSSPropertyFontStyle)) {
        if (!is<CSSPrimitiveValue>(*fontStyle))
            return;

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
            return;

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

    // Each item in the src property's list is a single CSSFontFaceSource. Put them all into a CSSFontFace.
    RefPtr<CSSFontFace> fontFace;

    int srcLength = srcList.length();

    bool foundSVGFont = false;

    for (int i = 0; i < srcLength; i++) {
        // An item in the list either specifies a string (local font name) or a URL (remote font to download).
        CSSFontFaceSrcValue& item = downcast<CSSFontFaceSrcValue>(*srcList.itemWithoutBoundsCheck(i));
        std::unique_ptr<CSSFontFaceSource> source;

#if ENABLE(SVG_FONTS)
        foundSVGFont = item.isSVGFontFaceSrc() || item.svgFontFaceElement();
#endif
        if (!item.isLocal()) {
            Settings* settings = m_document ? m_document->frame() ? &m_document->frame()->settings() : 0 : 0;
            bool allowDownloading = foundSVGFont || (settings && settings->downloadableBinaryFontsEnabled());
            if (allowDownloading && item.isSupportedFormat() && m_document) {
                CachedFont* cachedFont = item.cachedFont(m_document, foundSVGFont, isInitiatingElementInUserAgentShadowTree);
                if (cachedFont) {
                    source = std::make_unique<CSSFontFaceSource>(item.resource(), cachedFont);
#if ENABLE(SVG_FONTS)
                    if (foundSVGFont)
                        source->setHasExternalSVGFont();
#endif
                }
            }
        } else {
            source = std::make_unique<CSSFontFaceSource>(item.resource());
        }

        if (!fontFace) {
            RefPtr<CSSFontFaceRule> rule;
#if ENABLE(FONT_LOAD_EVENTS)
            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=112116 - This CSSFontFaceRule has no parent.
            if (RuntimeEnabledFeatures::sharedFeatures().fontLoadEventsEnabled())
                rule = static_pointer_cast<CSSFontFaceRule>(fontFaceRule->createCSSOMWrapper());
#endif
            fontFace = CSSFontFace::create(static_cast<FontTraitsMask>(traitsMask), rule);
        }

        if (source) {
#if ENABLE(SVG_FONTS)
            source->setSVGFontFaceElement(item.svgFontFaceElement());
#endif
            fontFace->addSource(WTF::move(source));
        }
    }

    ASSERT(fontFace);

    if (fontFace && !fontFace->isValid())
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
        applyValueFontVariantLigatures(*fontFace, *variantLigatures);

    if (variantPosition && is<CSSPrimitiveValue>(*variantPosition))
        fontFace->setVariantPosition(downcast<CSSPrimitiveValue>(*variantPosition));

    if (variantCaps && is<CSSPrimitiveValue>(*variantCaps))
        fontFace->setVariantCaps(downcast<CSSPrimitiveValue>(*variantCaps));

    if (variantNumeric)
        applyValueFontVariantNumeric(*fontFace, *variantNumeric);

    if (variantAlternates && is<CSSPrimitiveValue>(*variantAlternates))
        fontFace->setVariantAlternates(downcast<CSSPrimitiveValue>(*variantAlternates));

    if (variantEastAsian)
        applyValueFontVariantEastAsian(*fontFace, *variantEastAsian);

    // Hash under every single family name.
    for (auto& item : familyList) {
        auto& value = downcast<CSSPrimitiveValue>(item.get());
        String familyName;
        if (value.isFontFamily()) {
            familyName = value.fontFamily().familyName;
        } else if (value.isValueID()) {
            // We need to use the raw text for all the generic family types, since @font-face is a way of actually
            // defining what font to use for those types.
            switch (value.getValueID()) {
                case CSSValueSerif:
                    familyName = serifFamily;
                    break;
                case CSSValueSansSerif:
                    familyName = sansSerifFamily;
                    break;
                case CSSValueCursive:
                    familyName = cursiveFamily;
                    break;
                case CSSValueFantasy:
                    familyName = fantasyFamily;
                    break;
                case CSSValueMonospace:
                    familyName = monospaceFamily;
                    break;
                case CSSValueWebkitPictograph:
                    familyName = pictographFamily;
                    break;
                default:
                    break;
            }
        }

        if (familyName.isEmpty())
            continue;

        std::unique_ptr<Vector<RefPtr<CSSFontFace>>>& familyFontFaces = m_fontFaces.add(familyName, nullptr).iterator->value;
        if (!familyFontFaces) {
            familyFontFaces = std::make_unique<Vector<RefPtr<CSSFontFace>>>();

            ASSERT(!m_locallyInstalledFontFaces.contains(familyName));

            Vector<unsigned> locallyInstalledFontsTraitsMasks;
            FontCache::singleton().getTraitsInFamily(familyName, locallyInstalledFontsTraitsMasks);
            if (unsigned numLocallyInstalledFaces = locallyInstalledFontsTraitsMasks.size()) {
                auto familyLocallyInstalledFaces = std::make_unique<Vector<RefPtr<CSSFontFace>>>();

                for (unsigned i = 0; i < numLocallyInstalledFaces; ++i) {
                    RefPtr<CSSFontFace> locallyInstalledFontFace = CSSFontFace::create(static_cast<FontTraitsMask>(locallyInstalledFontsTraitsMasks[i]), 0, true);
                    locallyInstalledFontFace->addSource(std::make_unique<CSSFontFaceSource>(familyName));
                    ASSERT(locallyInstalledFontFace->isValid());
                    familyLocallyInstalledFaces->append(locallyInstalledFontFace);
                }

                m_locallyInstalledFontFaces.set(familyName, WTF::move(familyLocallyInstalledFaces));
            }
        }

        familyFontFaces->append(fontFace);
        
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

static FontTraitsMask desiredTraitsMaskForComparison;

static inline bool compareFontFaces(CSSFontFace* first, CSSFontFace* second)
{
    FontTraitsMask firstTraitsMask = first->traitsMask();
    FontTraitsMask secondTraitsMask = second->traitsMask();

    bool firstHasDesiredStyle = firstTraitsMask & desiredTraitsMaskForComparison & FontStyleMask;
    bool secondHasDesiredStyle = secondTraitsMask & desiredTraitsMaskForComparison & FontStyleMask;

    if (firstHasDesiredStyle != secondHasDesiredStyle)
        return firstHasDesiredStyle;

    if ((desiredTraitsMaskForComparison & FontStyleItalicMask) && !first->isLocalFallback() && !second->isLocalFallback()) {
        // Prefer a font that has indicated that it can only support italics to a font that claims to support
        // all styles.  The specialized font is more likely to be the one the author wants used.
        bool firstRequiresItalics = (firstTraitsMask & FontStyleItalicMask) && !(firstTraitsMask & FontStyleNormalMask);
        bool secondRequiresItalics = (secondTraitsMask & FontStyleItalicMask) && !(secondTraitsMask & FontStyleNormalMask);
        if (firstRequiresItalics != secondRequiresItalics)
            return firstRequiresItalics;
    }

    if (secondTraitsMask & desiredTraitsMaskForComparison & FontWeightMask)
        return false;
    if (firstTraitsMask & desiredTraitsMaskForComparison & FontWeightMask)
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
    unsigned w = FontWeight100Bit;
    while (!(desiredTraitsMaskForComparison & (1 << w))) {
        w++;
        ruleSetIndex++;
    }

    ASSERT_WITH_SECURITY_IMPLICATION(ruleSetIndex < fallbackRuleSets);
    const FontTraitsMask* weightFallbackRule = weightFallbackRuleSets[ruleSetIndex];
    for (unsigned i = 0; i < rulesPerSet; ++i) {
        if (secondTraitsMask & weightFallbackRule[i])
            return false;
        if (firstTraitsMask & weightFallbackRule[i])
            return true;
    }

    return false;
}

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
    Vector<RefPtr<CSSFontFace>>* familyFontFaces = m_fontFaces.get(family);
    if (!familyFontFaces || familyFontFaces->isEmpty())
        return 0;

    std::unique_ptr<HashMap<unsigned, RefPtr<CSSSegmentedFontFace>>>& segmentedFontFaceCache = m_fonts.add(family, nullptr).iterator->value;
    if (!segmentedFontFaceCache)
        segmentedFontFaceCache = std::make_unique<HashMap<unsigned, RefPtr<CSSSegmentedFontFace>>>();

    FontTraitsMask traitsMask = fontDescription.traitsMask();

    RefPtr<CSSSegmentedFontFace>& face = segmentedFontFaceCache->add(traitsMask, nullptr).iterator->value;
    if (!face) {
        face = CSSSegmentedFontFace::create(this);

        // Collect all matching faces and sort them in order of preference.
        Vector<CSSFontFace*, 32> candidateFontFaces;
        for (int i = familyFontFaces->size() - 1; i >= 0; --i) {
            CSSFontFace* candidate = familyFontFaces->at(i).get();
            unsigned candidateTraitsMask = candidate->traitsMask();
            if ((traitsMask & FontStyleNormalMask) && !(candidateTraitsMask & FontStyleNormalMask))
                continue;
            candidateFontFaces.append(candidate);
        }

        if (Vector<RefPtr<CSSFontFace>>* familyLocallyInstalledFontFaces = m_locallyInstalledFontFaces.get(family)) {
            unsigned numLocallyInstalledFontFaces = familyLocallyInstalledFontFaces->size();
            for (unsigned i = 0; i < numLocallyInstalledFontFaces; ++i) {
                CSSFontFace* candidate = familyLocallyInstalledFontFaces->at(i).get();
                unsigned candidateTraitsMask = candidate->traitsMask();
                if ((traitsMask & FontStyleNormalMask) && !(candidateTraitsMask & FontStyleNormalMask))
                    continue;
                candidateFontFaces.append(candidate);
            }
        }

        desiredTraitsMaskForComparison = traitsMask;
        std::stable_sort(candidateFontFaces.begin(), candidateFontFaces.end(), compareFontFaces);
        unsigned numCandidates = candidateFontFaces.size();
        for (unsigned i = 0; i < numCandidates; ++i)
            face->appendFontFace(candidateFontFaces[i]);
    }
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

bool CSSFontSelector::resolvesFamilyFor(const FontDescription& description) const
{
    for (unsigned i = 0; i < description.familyCount(); ++i) {
        const AtomicString& familyName = description.familyAt(i);
        if (familyName.isEmpty())
            continue;
        if (m_fontFaces.contains(familyName))
            return true;
        DEPRECATED_DEFINE_STATIC_LOCAL(String, webkitPrefix, ("-webkit-"));
        if (familyName.startsWith(webkitPrefix))
            return true;
            
    }
    return false;
}

size_t CSSFontSelector::fallbackFontCount()
{
    if (!m_document)
        return 0;

    if (Settings* settings = m_document->settings())
        return settings->fontFallbackPrefersPictographs() ? 1 : 0;

    return 0;
}

PassRefPtr<Font> CSSFontSelector::fallbackFontAt(const FontDescription& fontDescription, size_t index)
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
