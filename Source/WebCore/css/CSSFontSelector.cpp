/*
 * Copyright (C) 2007, 2008, 2011 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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
#include "CSSMutableStyleDeclaration.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyNames.h"
#include "CSSSegmentedFontFace.h"
#include "CSSStyleSelector.h"
#include "CSSUnicodeRangeValue.h"
#include "CSSValueKeywords.h"
#include "CSSValueList.h"
#include "CachedResourceLoader.h"
#include "Document.h"
#include "FontCache.h"
#include "FontFamilyValue.h"
#include "Frame.h"
#include "RenderObject.h"
#include "Settings.h"
#include "SimpleFontData.h"
#include "WebKitFontFamilyNames.h"
#include <wtf/text/AtomicString.h>

#if ENABLE(SVG)
#include "SVGFontFaceElement.h"
#include "SVGNames.h"
#endif

using namespace std;

namespace WebCore {

CSSFontSelector::CSSFontSelector(Document* document)
    : m_document(document)
    , m_beginLoadingTimer(this, &CSSFontSelector::beginLoadTimerFired)
{
    // FIXME: An old comment used to say there was no need to hold a reference to m_document
    // because "we are guaranteed to be destroyed before the document". But there does not
    // seem to be any such guarantee.

    ASSERT(m_document);
    fontCache()->addClient(this);
}

CSSFontSelector::~CSSFontSelector()
{
    clearDocument();
    fontCache()->removeClient(this);
}

bool CSSFontSelector::isEmpty() const
{
    return m_fonts.isEmpty();
}

void CSSFontSelector::addFontFaceRule(const CSSFontFaceRule* fontFaceRule)
{
    // Obtain the font-family property and the src property.  Both must be defined.
    const StylePropertySet* style = fontFaceRule->declaration();
    RefPtr<CSSValue> fontFamily = style->getPropertyCSSValue(CSSPropertyFontFamily);
    RefPtr<CSSValue> src = style->getPropertyCSSValue(CSSPropertySrc);
    RefPtr<CSSValue> unicodeRange = style->getPropertyCSSValue(CSSPropertyUnicodeRange);
    if (!fontFamily || !src || !fontFamily->isValueList() || !src->isValueList() || (unicodeRange && !unicodeRange->isValueList()))
        return;

    CSSValueList* familyList = static_cast<CSSValueList*>(fontFamily.get());
    if (!familyList->length())
        return;

    CSSValueList* srcList = static_cast<CSSValueList*>(src.get());
    if (!srcList->length())
        return;

    CSSValueList* rangeList = static_cast<CSSValueList*>(unicodeRange.get());

    unsigned traitsMask = 0;

    if (RefPtr<CSSValue> fontStyle = style->getPropertyCSSValue(CSSPropertyFontStyle)) {
        if (fontStyle->isPrimitiveValue()) {
            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            list->append(fontStyle);
            fontStyle = list;
        } else if (!fontStyle->isValueList())
            return;

        CSSValueList* styleList = static_cast<CSSValueList*>(fontStyle.get());
        unsigned numStyles = styleList->length();
        if (!numStyles)
            return;

        for (unsigned i = 0; i < numStyles; ++i) {
            switch (static_cast<CSSPrimitiveValue*>(styleList->itemWithoutBoundsCheck(i))->getIdent()) {
                case CSSValueAll:
                    traitsMask |= FontStyleMask;
                    break;
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
        }
    } else
        traitsMask |= FontStyleMask;

    if (RefPtr<CSSValue> fontWeight = style->getPropertyCSSValue(CSSPropertyFontWeight)) {
        if (fontWeight->isPrimitiveValue()) {
            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            list->append(fontWeight);
            fontWeight = list;
        } else if (!fontWeight->isValueList())
            return;

        CSSValueList* weightList = static_cast<CSSValueList*>(fontWeight.get());
        unsigned numWeights = weightList->length();
        if (!numWeights)
            return;

        for (unsigned i = 0; i < numWeights; ++i) {
            switch (static_cast<CSSPrimitiveValue*>(weightList->itemWithoutBoundsCheck(i))->getIdent()) {
                case CSSValueAll:
                    traitsMask |= FontWeightMask;
                    break;
                case CSSValueBolder:
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
                case CSSValueLighter:
                case CSSValue200:
                    traitsMask |= FontWeight200Mask;
                    break;
                case CSSValue100:
                    traitsMask |= FontWeight100Mask;
                    break;
                default:
                    break;
            }
        }
    } else
        traitsMask |= FontWeightMask;

    if (RefPtr<CSSValue> fontVariant = style->getPropertyCSSValue(CSSPropertyFontVariant)) {
        if (fontVariant->isPrimitiveValue()) {
            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            list->append(fontVariant);
            fontVariant = list;
        } else if (!fontVariant->isValueList())
            return;

        CSSValueList* variantList = static_cast<CSSValueList*>(fontVariant.get());
        unsigned numVariants = variantList->length();
        if (!numVariants)
            return;

        for (unsigned i = 0; i < numVariants; ++i) {
            switch (static_cast<CSSPrimitiveValue*>(variantList->itemWithoutBoundsCheck(i))->getIdent()) {
                case CSSValueAll:
                    traitsMask |= FontVariantMask;
                    break;
                case CSSValueNormal:
                    traitsMask |= FontVariantNormalMask;
                    break;
                case CSSValueSmallCaps:
                    traitsMask |= FontVariantSmallCapsMask;
                    break;
                default:
                    break;
            }
        }
    } else
        traitsMask |= FontVariantMask;

    // Each item in the src property's list is a single CSSFontFaceSource. Put them all into a CSSFontFace.
    RefPtr<CSSFontFace> fontFace;

    int srcLength = srcList->length();

    bool foundSVGFont = false;

    for (int i = 0; i < srcLength; i++) {
        // An item in the list either specifies a string (local font name) or a URL (remote font to download).
        CSSFontFaceSrcValue* item = static_cast<CSSFontFaceSrcValue*>(srcList->itemWithoutBoundsCheck(i));
        OwnPtr<CSSFontFaceSource> source;

#if ENABLE(SVG_FONTS)
        foundSVGFont = item->isSVGFontFaceSrc() || item->svgFontFaceElement();
#endif
        if (!item->isLocal()) {
            Settings* settings = m_document ? m_document->frame() ? m_document->frame()->settings() : 0 : 0;
            bool allowDownloading = foundSVGFont || (settings && settings->downloadableBinaryFontsEnabled());
            if (allowDownloading && item->isSupportedFormat() && m_document) {
                CachedFont* cachedFont = item->cachedFont(m_document);
                if (cachedFont) {
                    source = adoptPtr(new CSSFontFaceSource(item->resource(), cachedFont));
#if ENABLE(SVG_FONTS)
                    if (foundSVGFont)
                        source->setHasExternalSVGFont(true);
#endif
                }
            }
        } else {
            source = adoptPtr(new CSSFontFaceSource(item->resource()));
        }

        if (!fontFace)
            fontFace = CSSFontFace::create(static_cast<FontTraitsMask>(traitsMask));

        if (source) {
#if ENABLE(SVG_FONTS)
            source->setSVGFontFaceElement(item->svgFontFaceElement());
#endif
            fontFace->addSource(source.release());
        }
    }

    ASSERT(fontFace);

    if (fontFace && !fontFace->isValid())
        return;

    if (rangeList) {
        unsigned numRanges = rangeList->length();
        for (unsigned i = 0; i < numRanges; i++) {
            CSSUnicodeRangeValue* range = static_cast<CSSUnicodeRangeValue*>(rangeList->itemWithoutBoundsCheck(i));
            fontFace->addRange(range->from(), range->to());
        }
    }

    // Hash under every single family name.
    int familyLength = familyList->length();
    for (int i = 0; i < familyLength; i++) {
        CSSPrimitiveValue* item = static_cast<CSSPrimitiveValue*>(familyList->itemWithoutBoundsCheck(i));
        String familyName;
        if (item->isString())
            familyName = static_cast<FontFamilyValue*>(item)->familyName();
        else if (item->isIdent()) {
            // We need to use the raw text for all the generic family types, since @font-face is a way of actually
            // defining what font to use for those types.
            String familyName;
            switch (item->getIdent()) {
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

        OwnPtr<Vector<RefPtr<CSSFontFace> > >& familyFontFaces = m_fontFaces.add(familyName, nullptr).first->second;
        if (!familyFontFaces) {
            familyFontFaces = adoptPtr(new Vector<RefPtr<CSSFontFace> >);

            ASSERT(!m_locallyInstalledFontFaces.contains(familyName));

            Vector<unsigned> locallyInstalledFontsTraitsMasks;
            fontCache()->getTraitsInFamily(familyName, locallyInstalledFontsTraitsMasks);
            if (unsigned numLocallyInstalledFaces = locallyInstalledFontsTraitsMasks.size()) {
                OwnPtr<Vector<RefPtr<CSSFontFace> > > familyLocallyInstalledFaces = adoptPtr(new Vector<RefPtr<CSSFontFace> >);

                for (unsigned i = 0; i < numLocallyInstalledFaces; ++i) {
                    RefPtr<CSSFontFace> locallyInstalledFontFace = CSSFontFace::create(static_cast<FontTraitsMask>(locallyInstalledFontsTraitsMasks[i]), true);
                    locallyInstalledFontFace->addSource(adoptPtr(new CSSFontFaceSource(familyName)));
                    ASSERT(locallyInstalledFontFace->isValid());
                    familyLocallyInstalledFaces->append(locallyInstalledFontFace);
                }

                m_locallyInstalledFontFaces.set(familyName, familyLocallyInstalledFaces.release());
            }
        }

        familyFontFaces->append(fontFace);
    }
}

void CSSFontSelector::registerForInvalidationCallbacks(FontSelectorClient* client)
{
    m_clients.add(client);
}

void CSSFontSelector::unregisterForInvalidationCallbacks(FontSelectorClient* client)
{
    m_clients.remove(client);
}

void CSSFontSelector::dispatchInvalidationCallbacks()
{
    Vector<FontSelectorClient*> clients;
    copyToVector(m_clients, clients);
    for (size_t i = 0; i < clients.size(); ++i)
        clients[i]->fontsNeedUpdate(this);

    // FIXME: Make Document a FontSelectorClient so that it can simply register for invalidation callbacks.
    if (!m_document)
        return;
    if (CSSStyleSelector* styleSelector = m_document->styleSelectorIfExists())
        styleSelector->invalidateMatchedDeclarationCache();
    if (m_document->inPageCache() || !m_document->renderer())
        return;
    m_document->scheduleForcedStyleRecalc();
}

void CSSFontSelector::fontLoaded()
{
    dispatchInvalidationCallbacks();
}

void CSSFontSelector::fontCacheInvalidated()
{
    dispatchInvalidationCallbacks();
}

static FontData* fontDataForGenericFamily(Document* document, const FontDescription& fontDescription, const AtomicString& familyName)
{
    if (!document || !document->frame())
        return 0;

    const Settings* settings = document->frame()->settings();
    if (!settings)
        return 0;

    AtomicString genericFamily;
    UScriptCode script = fontDescription.script();

    if (familyName == serifFamily)
         genericFamily = settings->serifFontFamily(script);
    else if (familyName == sansSerifFamily)
         genericFamily = settings->sansSerifFontFamily(script);
    else if (familyName == cursiveFamily)
         genericFamily = settings->cursiveFontFamily(script);
    else if (familyName == fantasyFamily)
         genericFamily = settings->fantasyFontFamily(script);
    else if (familyName == monospaceFamily)
         genericFamily = settings->fixedFontFamily(script);
    else if (familyName == pictographFamily)
         genericFamily = settings->pictographFontFamily(script);
    else if (familyName == standardFamily)
         genericFamily = settings->standardFontFamily(script);

    if (!genericFamily.isEmpty())
        return fontCache()->getCachedFontData(fontDescription, genericFamily);

    return 0;
}

static FontTraitsMask desiredTraitsMaskForComparison;

static inline bool compareFontFaces(CSSFontFace* first, CSSFontFace* second)
{
    FontTraitsMask firstTraitsMask = first->traitsMask();
    FontTraitsMask secondTraitsMask = second->traitsMask();

    bool firstHasDesiredVariant = firstTraitsMask & desiredTraitsMaskForComparison & FontVariantMask;
    bool secondHasDesiredVariant = secondTraitsMask & desiredTraitsMaskForComparison & FontVariantMask;

    if (firstHasDesiredVariant != secondHasDesiredVariant)
        return firstHasDesiredVariant;

    if ((desiredTraitsMaskForComparison & FontVariantSmallCapsMask) && !first->isLocalFallback() && !second->isLocalFallback()) {
        // Prefer a font that has indicated that it can only support small-caps to a font that claims to support
        // all variants.  The specialized font is more likely to be true small-caps and not require synthesis.
        bool firstRequiresSmallCaps = (firstTraitsMask & FontVariantSmallCapsMask) && !(firstTraitsMask & FontVariantNormalMask);
        bool secondRequiresSmallCaps = (secondTraitsMask & FontVariantSmallCapsMask) && !(secondTraitsMask & FontVariantNormalMask);
        if (firstRequiresSmallCaps != secondRequiresSmallCaps)
            return firstRequiresSmallCaps;
    }

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

    // http://www.w3.org/TR/2002/WD-css3-webfonts-20020802/#q46 says: "If there are fewer then 9 weights in the family, the default algorithm
    // for filling the "holes" is as follows. If '500' is unassigned, it will be assigned the same font as '400'. If any of the values '600',
    // '700', '800', or '900' remains unassigned, they are assigned to the same face as the next darker assigned keyword, if any, or the next
    // lighter one otherwise. If any of '300', '200', or '100' remains unassigned, it is assigned to the next lighter assigned keyword, if any,
    // or the next darker otherwise."
    // For '400', we made up our own rule (which then '500' follows).

    static const unsigned fallbackRuleSets = 9;
    static const unsigned rulesPerSet = 8;
    static const FontTraitsMask weightFallbackRuleSets[fallbackRuleSets][rulesPerSet] = {
        { FontWeight200Mask, FontWeight300Mask, FontWeight400Mask, FontWeight500Mask, FontWeight600Mask, FontWeight700Mask, FontWeight800Mask, FontWeight900Mask },
        { FontWeight100Mask, FontWeight300Mask, FontWeight400Mask, FontWeight500Mask, FontWeight600Mask, FontWeight700Mask, FontWeight800Mask, FontWeight900Mask },
        { FontWeight200Mask, FontWeight100Mask, FontWeight400Mask, FontWeight500Mask, FontWeight600Mask, FontWeight700Mask, FontWeight800Mask, FontWeight900Mask },
        { FontWeight500Mask, FontWeight300Mask, FontWeight600Mask, FontWeight200Mask, FontWeight700Mask, FontWeight100Mask, FontWeight800Mask, FontWeight900Mask },
        { FontWeight400Mask, FontWeight300Mask, FontWeight600Mask, FontWeight200Mask, FontWeight700Mask, FontWeight100Mask, FontWeight800Mask, FontWeight900Mask },
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

    ASSERT(ruleSetIndex < fallbackRuleSets);
    const FontTraitsMask* weightFallbackRule = weightFallbackRuleSets[ruleSetIndex];
    for (unsigned i = 0; i < rulesPerSet; ++i) {
        if (secondTraitsMask & weightFallbackRule[i])
            return false;
        if (firstTraitsMask & weightFallbackRule[i])
            return true;
    }

    return false;
}

FontData* CSSFontSelector::getFontData(const FontDescription& fontDescription, const AtomicString& familyName)
{
    if (m_fontFaces.isEmpty()) {
        if (familyName.startsWith("-webkit-"))
            return fontDataForGenericFamily(m_document, fontDescription, familyName);
        if (fontDescription.genericFamily() == FontDescription::StandardFamily && !fontDescription.isSpecifiedFont())
            return fontDataForGenericFamily(m_document, fontDescription, "-webkit-standard");
        return 0;
    }

    String family = familyName.string();

    Vector<RefPtr<CSSFontFace> >* familyFontFaces = m_fontFaces.get(family);
    // If no face was found, then return 0 and let the OS come up with its best match for the name.
    if (!familyFontFaces || familyFontFaces->isEmpty()) {
        // If we were handed a generic family, but there was no match, go ahead and return the correct font based off our
        // settings.
        if (fontDescription.genericFamily() == FontDescription::StandardFamily && !fontDescription.isSpecifiedFont())
            return fontDataForGenericFamily(m_document, fontDescription, "-webkit-standard");
        return fontDataForGenericFamily(m_document, fontDescription, familyName);
    }

    OwnPtr<HashMap<unsigned, RefPtr<CSSSegmentedFontFace> > >& segmentedFontFaceCache = m_fonts.add(family, nullptr).first->second;
    if (!segmentedFontFaceCache)
        segmentedFontFaceCache = adoptPtr(new HashMap<unsigned, RefPtr<CSSSegmentedFontFace> >);

    FontTraitsMask traitsMask = fontDescription.traitsMask();

    RefPtr<CSSSegmentedFontFace>& face = segmentedFontFaceCache->add(traitsMask, 0).first->second;
    if (!face) {
        face = CSSSegmentedFontFace::create(this);

        // Collect all matching faces and sort them in order of preference.
        Vector<CSSFontFace*, 32> candidateFontFaces;
        for (int i = familyFontFaces->size() - 1; i >= 0; --i) {
            CSSFontFace* candidate = familyFontFaces->at(i).get();
            unsigned candidateTraitsMask = candidate->traitsMask();
            if ((traitsMask & FontStyleNormalMask) && !(candidateTraitsMask & FontStyleNormalMask))
                continue;
            if ((traitsMask & FontVariantNormalMask) && !(candidateTraitsMask & FontVariantNormalMask))
                continue;
#if ENABLE(SVG_FONTS)
            // For SVG Fonts that specify that they only support the "normal" variant, we will assume they are incapable
            // of small-caps synthesis and just ignore the font face as a candidate.
            if (candidate->hasSVGFontFaceSource() && (traitsMask & FontVariantSmallCapsMask) && !(candidateTraitsMask & FontVariantSmallCapsMask))
                continue;
#endif
            candidateFontFaces.append(candidate);
        }

        if (Vector<RefPtr<CSSFontFace> >* familyLocallyInstalledFontFaces = m_locallyInstalledFontFaces.get(family)) {
            unsigned numLocallyInstalledFontFaces = familyLocallyInstalledFontFaces->size();
            for (unsigned i = 0; i < numLocallyInstalledFontFaces; ++i) {
                CSSFontFace* candidate = familyLocallyInstalledFontFaces->at(i).get();
                unsigned candidateTraitsMask = candidate->traitsMask();
                if ((traitsMask & FontStyleNormalMask) && !(candidateTraitsMask & FontStyleNormalMask))
                    continue;
                if ((traitsMask & FontVariantNormalMask) && !(candidateTraitsMask & FontVariantNormalMask))
                    continue;
                candidateFontFaces.append(candidate);
            }
        }

        desiredTraitsMaskForComparison = traitsMask;
        stable_sort(candidateFontFaces.begin(), candidateFontFaces.end(), compareFontFaces);
        unsigned numCandidates = candidateFontFaces.size();
        for (unsigned i = 0; i < numCandidates; ++i)
            face->appendFontFace(candidateFontFaces[i]);
    }

    // We have a face.  Ask it for a font data.  If it cannot produce one, it will fail, and the OS will take over.
    return face->getFontData(fontDescription);
}

void CSSFontSelector::clearDocument()
{
    if (!m_document) {
        ASSERT(!m_beginLoadingTimer.isActive());
        ASSERT(m_fontsToBeginLoading.isEmpty());
        return;
    }

    m_beginLoadingTimer.stop();

    CachedResourceLoader* cachedResourceLoader = m_document->cachedResourceLoader();
    for (size_t i = 0; i < m_fontsToBeginLoading.size(); ++i) {
        // Balances incrementRequestCount() in beginLoadingFontSoon().
        cachedResourceLoader->decrementRequestCount(m_fontsToBeginLoading[i].get());
    }

    m_fontsToBeginLoading.clear();

    m_document = 0;
}

void CSSFontSelector::beginLoadingFontSoon(CachedFont* font)
{
    if (!m_document)
        return;

    m_fontsToBeginLoading.append(font);
    // Increment the request count now, in order to prevent didFinishLoad from being dispatched
    // after this font has been requested but before it began loading. Balanced by
    // decrementRequestCount() in beginLoadTimerFired() and in clearDocument().
    m_document->cachedResourceLoader()->incrementRequestCount(font);
    m_beginLoadingTimer.startOneShot(0);
}

void CSSFontSelector::beginLoadTimerFired(Timer<WebCore::CSSFontSelector>*)
{
    Vector<CachedResourceHandle<CachedFont> > fontsToBeginLoading;
    fontsToBeginLoading.swap(m_fontsToBeginLoading);

    CachedResourceLoader* cachedResourceLoader = m_document->cachedResourceLoader();
    for (size_t i = 0; i < fontsToBeginLoading.size(); ++i) {
        fontsToBeginLoading[i]->beginLoadIfNeeded(cachedResourceLoader);
        // Balances incrementRequestCount() in beginLoadingFontSoon().
        cachedResourceLoader->decrementRequestCount(fontsToBeginLoading[i].get());
    }
    // Ensure that if the request count reaches zero, the frame loader will know about it.
    cachedResourceLoader->loadDone();
}

}
