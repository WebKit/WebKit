/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "CSSFontFaceSet.h"

#include "CSSFontFaceSource.h"
#include "CSSFontFamily.h"
#include "CSSFontSelector.h"
#include "CSSParser.h"
#include "CSSPrimitiveValue.h"
#include "CSSSegmentedFontFace.h"
#include "CSSValueList.h"
#include "CSSValuePool.h"
#include "FontCache.h"
#include "StyleProperties.h"

namespace WebCore {

CSSFontFaceSet::CSSFontFaceSet()
{
}

CSSFontFaceSet::~CSSFontFaceSet()
{
    for (auto& face : m_faces)
        face->removeClient(*this);

    for (auto& pair : m_locallyInstalledFacesLookupTable) {
        for (auto& face : pair.value)
            face->removeClient(*this);
    }
}

void CSSFontFaceSet::addClient(CSSFontFaceSetClient& client)
{
    m_clients.add(&client);
}

void CSSFontFaceSet::removeClient(CSSFontFaceSetClient& client)
{
    ASSERT(m_clients.contains(&client));
    m_clients.remove(&client);
}

void CSSFontFaceSet::incrementActiveCount()
{
    ++m_activeCount;
    if (m_activeCount == 1) {
        m_status = Status::Loading;
        for (auto* client : m_clients)
            client->startedLoading();
    }
}

void CSSFontFaceSet::decrementActiveCount()
{
    --m_activeCount;
    if (!m_activeCount) {
        m_status = Status::Loaded;
        for (auto* client : m_clients)
            client->completedLoading();
    }
}

bool CSSFontFaceSet::hasFace(const CSSFontFace& face) const
{
    for (auto& myFace : m_faces) {
        if (myFace.ptr() == &face)
            return true;
    }

    return false;
}

void CSSFontFaceSet::registerLocalFontFacesForFamily(const String& familyName)
{
    ASSERT(!m_locallyInstalledFacesLookupTable.contains(familyName));

    Vector<FontTraitsMask> traitsMasks = FontCache::singleton().getTraitsInFamily(familyName);
    if (traitsMasks.isEmpty())
        return;

    Vector<Ref<CSSFontFace>> faces;
    for (auto mask : traitsMasks) {
        Ref<CSSFontFace> face = CSSFontFace::create(nullptr, nullptr, nullptr, true);
        
        Ref<CSSValueList> familyList = CSSValueList::createCommaSeparated();
        familyList->append(CSSValuePool::singleton().createFontFamilyValue(familyName));
        face->setFamilies(familyList.get());
        face->setTraitsMask(mask);
        face->adoptSource(std::make_unique<CSSFontFaceSource>(face.get(), familyName));
        ASSERT(!face->allSourcesFailed());
        faces.append(WTFMove(face));
    }
    m_locallyInstalledFacesLookupTable.add(familyName, WTFMove(faces));
}

String CSSFontFaceSet::familyNameFromPrimitive(const CSSPrimitiveValue& value)
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

void CSSFontFaceSet::addToFacesLookupTable(CSSFontFace& face)
{
    if (!face.families())
        return;

    for (auto& item : *face.families()) {
        String familyName = CSSFontFaceSet::familyNameFromPrimitive(downcast<CSSPrimitiveValue>(item.get()));
        if (familyName.isEmpty())
            continue;

        auto addResult = m_facesLookupTable.add(familyName, Vector<Ref<CSSFontFace>>());
        auto& familyFontFaces = addResult.iterator->value;
        if (addResult.isNewEntry) {
            // m_locallyInstalledFontFaces grows without bound, eventually encorporating every font installed on the system.
            // This is by design.
            registerLocalFontFacesForFamily(familyName);
            familyFontFaces = { };
        }

        familyFontFaces.append(face);
    }
}

void CSSFontFaceSet::add(CSSFontFace& face)
{
    ASSERT(!hasFace(face));

    for (auto* client : m_clients)
        client->fontModified();

    face.addClient(*this);
    m_cache.clear();

    if (face.cssConnection())
        m_faces.insert(m_facesPartitionIndex++, face);
    else
        m_faces.append(face);

    addToFacesLookupTable(face);

    if (face.status() == CSSFontFace::Status::Loading || face.status() == CSSFontFace::Status::TimedOut)
        incrementActiveCount();
}

void CSSFontFaceSet::removeFromFacesLookupTable(const CSSFontFace& face, const CSSValueList& familiesToSearchFor)
{
    for (auto& item : familiesToSearchFor) {
        String familyName = CSSFontFaceSet::familyNameFromPrimitive(downcast<CSSPrimitiveValue>(item.get()));
        if (familyName.isEmpty())
            continue;

        auto iterator = m_facesLookupTable.find(familyName);
        ASSERT(iterator != m_facesLookupTable.end());
        bool found = false;
        for (size_t i = 0; i < iterator->value.size(); ++i) {
            if (iterator->value[i].ptr() == &face) {
                found = true;
                iterator->value.remove(i);
                break;
            }
        }
        ASSERT_UNUSED(found, found);
        if (!iterator->value.size())
            m_facesLookupTable.remove(iterator);
    }
}

void CSSFontFaceSet::remove(const CSSFontFace& face)
{
    m_cache.clear();

    for (auto* client : m_clients)
        client->fontModified();

    if (face.families())
        removeFromFacesLookupTable(face, *face.families());

    for (size_t i = 0; i < m_faces.size(); ++i) {
        if (m_faces[i].ptr() == &face) {
            if (i < m_facesPartitionIndex)
                --m_facesPartitionIndex;
            m_faces[i]->removeClient(*this);
            m_faces.remove(i);
            if (face.status() == CSSFontFace::Status::Loading || face.status() == CSSFontFace::Status::TimedOut)
                decrementActiveCount();
            return;
        }
    }
    ASSERT_NOT_REACHED();
}

void CSSFontFaceSet::clear()
{
    m_faces.clear();
    m_facesLookupTable.clear();
    m_locallyInstalledFacesLookupTable.clear();
    m_cache.clear();
}

CSSFontFace& CSSFontFaceSet::operator[](size_t i)
{
    ASSERT(i < faceCount());
    return m_faces[i];
}

static Optional<FontTraitsMask> computeFontTraitsMask(MutableStyleProperties& style)
{
    RefPtr<CSSValue> styleValue = style.getPropertyCSSValue(CSSPropertyFontStyle).get();
    if (!styleValue)
        styleValue = CSSValuePool::singleton().createIdentifierValue(CSSValueNormal).ptr();

    FontTraitsMask styleMask;
    if (auto styleMaskOptional = CSSFontFace::calculateStyleMask(*styleValue))
        styleMask = styleMaskOptional.value();
    else
        return Nullopt;

    RefPtr<CSSValue> weightValue = style.getPropertyCSSValue(CSSPropertyFontWeight).get();
    if (!weightValue)
        weightValue = CSSValuePool::singleton().createIdentifierValue(CSSValueNormal).ptr();

    FontTraitsMask weightMask;
    if (auto weightMaskOptional = CSSFontFace::calculateWeightMask(*weightValue))
        weightMask = weightMaskOptional.value();
    else
        return Nullopt;

    return static_cast<FontTraitsMask>(static_cast<unsigned>(styleMask) | static_cast<unsigned>(weightMask));
}

Vector<std::reference_wrapper<CSSFontFace>> CSSFontFaceSet::matchingFaces(const String& font, const String&, ExceptionCode& ec)
{
    Vector<std::reference_wrapper<CSSFontFace>> result;
    Ref<MutableStyleProperties> style = MutableStyleProperties::create();
    auto parseResult = CSSParser::parseValue(style.ptr(), CSSPropertyFont, font, true, CSSStrictMode, nullptr);
    if (parseResult == CSSParser::ParseResult::Error) {
        ec = SYNTAX_ERR;
        return result;
    }

    FontTraitsMask fontTraitsMask;
    if (auto maskOptional = computeFontTraitsMask(style.get()))
        fontTraitsMask = maskOptional.value();
    else {
        ec = SYNTAX_ERR;
        return result;
    }

    RefPtr<CSSValue> family = style->getPropertyCSSValue(CSSPropertyFontFamily);
    if (!is<CSSValueList>(family.get())) {
        ec = SYNTAX_ERR;
        return result;
    }
    CSSValueList& familyList = downcast<CSSValueList>(*family);

    HashSet<AtomicString> uniqueFamilies;
    for (auto& family : familyList) {
        const CSSPrimitiveValue& primitive = downcast<CSSPrimitiveValue>(family.get());
        if (!primitive.isFontFamily())
            continue;
        uniqueFamilies.add(primitive.fontFamily().familyName);
    }

    for (auto& family : uniqueFamilies) {
        CSSSegmentedFontFace* faces = getFontFace(fontTraitsMask, family);
        if (!faces)
            continue;
        for (auto& constituentFace : faces->constituentFaces())
            result.append(constituentFace.get());
    }

    return result;
}

bool CSSFontFaceSet::check(const String& font, const String& text, ExceptionCode& ec)
{
    auto matchingFaces = this->matchingFaces(font, text, ec);
    if (ec)
        return false;

    for (auto& face : matchingFaces) {
        if (face.get().status() == CSSFontFace::Status::Pending)
            return false;
    }
    return true;
}

static bool fontFaceComparator(FontTraitsMask desiredTraitsMaskForComparison, const CSSFontFace& first, const CSSFontFace& second)
{
    FontTraitsMask firstTraitsMask = first.traitsMask();
    FontTraitsMask secondTraitsMask = second.traitsMask();

    bool firstHasDesiredStyle = firstTraitsMask & desiredTraitsMaskForComparison & FontStyleMask;
    bool secondHasDesiredStyle = secondTraitsMask & desiredTraitsMaskForComparison & FontStyleMask;

    if (firstHasDesiredStyle != secondHasDesiredStyle)
        return firstHasDesiredStyle;

    if ((desiredTraitsMaskForComparison & FontStyleItalicMask) && !first.isLocalFallback() && !second.isLocalFallback()) {
        // Prefer a font that has indicated that it can only support italics to a font that claims to support
        // all styles. The specialized font is more likely to be the one the author wants used.
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
    for (; !(desiredTraitsMaskForComparison & (1 << (FontWeight100Bit + ruleSetIndex))); ruleSetIndex++) { }

    const FontTraitsMask* weightFallbackRule = weightFallbackRuleSets[ruleSetIndex];
    for (unsigned i = 0; i < rulesPerSet; ++i) {
        if (secondTraitsMask & weightFallbackRule[i])
            return false;
        if (firstTraitsMask & weightFallbackRule[i])
            return true;
    }

    return false;
}

CSSSegmentedFontFace* CSSFontFaceSet::getFontFace(FontTraitsMask traitsMask, const AtomicString& family)
{
    auto iterator = m_facesLookupTable.find(family);
    if (iterator == m_facesLookupTable.end())
        return nullptr;
    auto& familyFontFaces = iterator->value;

    auto& segmentedFontFaceCache = m_cache.add(family, HashMap<unsigned, std::unique_ptr<CSSSegmentedFontFace>>()).iterator->value;

    auto& face = segmentedFontFaceCache.add(traitsMask, nullptr).iterator->value;
    if (face)
        return face.get();

    face = std::make_unique<CSSSegmentedFontFace>();

    Vector<std::reference_wrapper<CSSFontFace>, 32> candidateFontFaces;
    for (int i = familyFontFaces.size() - 1; i >= 0; --i) {
        CSSFontFace& candidate = familyFontFaces[i];
        unsigned candidateTraitsMask = candidate.traitsMask();
        if ((traitsMask & FontStyleNormalMask) && !(candidateTraitsMask & FontStyleNormalMask))
            continue;
        candidateFontFaces.append(candidate);
    }

    auto localIterator = m_locallyInstalledFacesLookupTable.find(family);
    if (localIterator != m_locallyInstalledFacesLookupTable.end()) {
        for (auto& candidate : localIterator->value) {
            unsigned candidateTraitsMask = candidate->traitsMask();
            if ((traitsMask & FontStyleNormalMask) && !(candidateTraitsMask & FontStyleNormalMask))
                continue;
            candidateFontFaces.append(candidate);
        }
    }

    std::stable_sort(candidateFontFaces.begin(), candidateFontFaces.end(), [traitsMask](const CSSFontFace& first, const CSSFontFace& second) {
        return fontFaceComparator(traitsMask, first, second);
    });
    for (auto& candidate : candidateFontFaces)
        face->appendFontFace(candidate.get());

    return face.get();
}

void CSSFontFaceSet::fontStateChanged(CSSFontFace& face, CSSFontFace::Status oldState, CSSFontFace::Status newState)
{
    ASSERT(hasFace(face));
    if (oldState == CSSFontFace::Status::Pending) {
        ASSERT(newState == CSSFontFace::Status::Loading);
        incrementActiveCount();
    }
    if (newState == CSSFontFace::Status::Success || newState == CSSFontFace::Status::Failure) {
        ASSERT(oldState == CSSFontFace::Status::Loading || oldState == CSSFontFace::Status::TimedOut);
        for (auto* client : m_clients)
            client->faceFinished(face, newState);
        decrementActiveCount();
    }
}

void CSSFontFaceSet::fontPropertyChanged(CSSFontFace& face, CSSValueList* oldFamilies)
{
    m_cache.clear();

    if (oldFamilies) {
        removeFromFacesLookupTable(face, *oldFamilies);
        addToFacesLookupTable(face);
    }

    for (auto* client : m_clients)
        client->fontModified();
}

}
