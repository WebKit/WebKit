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
#include "CSSFontStyleValue.h"
#include "CSSParser.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyParserHelpers.h"
#include "CSSPropertyParserWorkerSafe.h"
#include "CSSSegmentedFontFace.h"
#include "CSSValueList.h"
#include "CSSValuePool.h"
#include "FontCache.h"
#include "StyleBuilderConverter.h"
#include "StyleProperties.h"

namespace WebCore {

CSSFontFaceSet::CSSFontFaceSet(CSSFontSelector* owningFontSelector)
    : m_owningFontSelector(owningFontSelector)
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

void CSSFontFaceSet::addFontModifiedObserver(const FontModifiedObserver& fontModifiedObserver)
{
    auto result = m_fontModifiedObservers.add(fontModifiedObserver);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void CSSFontFaceSet::addFontEventClient(const FontEventClient& fontEventClient)
{
    auto result = m_fontEventClients.add(fontEventClient);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void CSSFontFaceSet::incrementActiveCount()
{
    ++m_activeCount;
    if (m_activeCount == 1) {
        m_status = Status::Loading;
        m_fontEventClients.forEach([] (auto& client) {
            client.startedLoading();
        });
    }
}

void CSSFontFaceSet::decrementActiveCount()
{
    --m_activeCount;
    if (!m_activeCount) {
        m_status = Status::Loaded;
        m_fontEventClients.forEach([] (auto& client) {
            client.completedLoading();
        });
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

// Calling updateStyleIfNeeded() might delete |this|.
void CSSFontFaceSet::updateStyleIfNeeded()
{
    if (m_owningFontSelector)
        m_owningFontSelector->updateStyleIfNeeded();
}

void CSSFontFaceSet::ensureLocalFontFacesForFamilyRegistered(const String& familyName)
{
    ASSERT(m_owningFontSelector);
    if (m_locallyInstalledFacesLookupTable.contains(familyName))
        return;

    if (!m_owningFontSelector->scriptExecutionContext())
        return;
    AllowUserInstalledFonts allowUserInstalledFonts = m_owningFontSelector->scriptExecutionContext()->settingsValues().shouldAllowUserInstalledFonts ? AllowUserInstalledFonts::Yes : AllowUserInstalledFonts::No;
    Vector<FontSelectionCapabilities> capabilities = FontCache::forCurrentThread().getFontSelectionCapabilitiesInFamily(AtomString { familyName }, allowUserInstalledFonts);
    if (capabilities.isEmpty())
        return;

    Vector<Ref<CSSFontFace>> faces;
    for (auto item : capabilities) {
        auto face = CSSFontFace::create(*m_owningFontSelector, nullptr, nullptr, true);
        
        // FIXME: Don't use a list here. https://bugs.webkit.org/show_bug.cgi?id=196381
        Ref<CSSValueList> familyList = CSSValueList::createCommaSeparated();
        familyList->append(m_owningFontSelector->scriptExecutionContext()->cssValuePool().createFontFamilyValue(familyName));
        face->setFamilies(familyList.get());
        face->setFontSelectionCapabilities(item);
        face->adoptSource(makeUnique<CSSFontFaceSource>(face.get(), familyName));
        ASSERT(!face->computeFailureState());
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
    switch (value.valueID()) {
    case CSSValueSerif:
        return serifFamily.get();
    case CSSValueSansSerif:
        return sansSerifFamily.get();
    case CSSValueCursive:
        return cursiveFamily.get();
    case CSSValueFantasy:
        return fantasyFamily.get();
    case CSSValueMonospace:
        return monospaceFamily.get();
    case CSSValueWebkitPictograph:
        return pictographFamily.get();
    case CSSValueSystemUi:
        return systemUiFamily.get();
    default:
        return { };
    }
}

void CSSFontFaceSet::addToFacesLookupTable(CSSFontFace& face)
{
    auto families = face.families();
    if (!families) {
        // If the font has failed, there's no point in actually adding it to m_facesLookupTable,
        // because no font requests can actually use it for anything. So, let's just ... not add it.
        return;
    }

    for (auto& item : *families) {
        String familyName = CSSFontFaceSet::familyNameFromPrimitive(downcast<CSSPrimitiveValue>(item.get()));
        if (familyName.isEmpty())
            continue;

        auto addResult = m_facesLookupTable.add(familyName, Vector<Ref<CSSFontFace>>());
        auto& familyFontFaces = addResult.iterator->value;
        if (addResult.isNewEntry) {
            // m_locallyInstalledFontFaces grows without bound, eventually encorporating every font installed on the system.
            // This is by design.
            if (m_owningFontSelector)
                ensureLocalFontFacesForFamilyRegistered(familyName);
            familyFontFaces = { };
        }

        familyFontFaces.append(face);
    }
}

void CSSFontFaceSet::add(CSSFontFace& face)
{
    ASSERT(!hasFace(face));

    m_fontModifiedObservers.forEach([] (auto& observer) {
        observer();
    });

    face.addClient(*this);
    m_cache.clear();

    if (face.cssConnection())
        m_faces.insert(m_facesPartitionIndex++, face);
    else
        m_faces.append(face);

    addToFacesLookupTable(face);

    if (face.status() == CSSFontFace::Status::Loading || face.status() == CSSFontFace::Status::TimedOut)
        incrementActiveCount();

    if (face.cssConnection()) {
        ASSERT(!m_constituentCSSConnections.contains(face.cssConnection()));
        m_constituentCSSConnections.add(face.cssConnection(), &face);
    }
}

void CSSFontFaceSet::removeFromFacesLookupTable(const CSSFontFace& face, const CSSValueList& familiesToSearchFor)
{
    for (auto& item : familiesToSearchFor) {
        String familyName = CSSFontFaceSet::familyNameFromPrimitive(downcast<CSSPrimitiveValue>(item.get()));
        if (familyName.isEmpty())
            continue;

        auto iterator = m_facesLookupTable.find(familyName);
        if (iterator == m_facesLookupTable.end()) {
            // The font may have failed even before addToFacesLookupTable() was called on it,
            // which means we never added it (because there's no point in adding a failed font).
            // So, if it was never added, removing it is free! Woohoo!
            return;
        }
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
    Ref protect { face };

    m_cache.clear();

    m_fontModifiedObservers.forEach([] (auto& observer) {
        observer();
    });
    
    if (auto families = face.families())
        removeFromFacesLookupTable(face, *families);

    if (face.cssConnection()) {
        ASSERT(m_constituentCSSConnections.get(face.cssConnection()) == &face);
        m_constituentCSSConnections.remove(face.cssConnection());
    }

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

CSSFontFace* CSSFontFaceSet::lookUpByCSSConnection(StyleRuleFontFace& target)
{
    return m_constituentCSSConnections.get(&target);
}

void CSSFontFaceSet::purge()
{
    Vector<Ref<CSSFontFace>> toRemove;
    for (auto& face : m_faces) {
        if (face->purgeable())
            toRemove.append(face.copyRef());
    }

    for (auto& item : toRemove)
        remove(item.get());
}

void CSSFontFaceSet::emptyCaches()
{
    m_cache.clear();
}

void CSSFontFaceSet::clear()
{
    for (auto& face : m_faces)
        face->removeClient(*this);
    m_faces.clear();
    m_facesLookupTable.clear();
    m_locallyInstalledFacesLookupTable.clear();
    m_cache.clear();
    m_constituentCSSConnections.clear();
    m_facesPartitionIndex = 0;
    m_status = Status::Loaded;
}

CSSFontFace& CSSFontFaceSet::operator[](size_t i)
{
    ASSERT(i < faceCount());
    return m_faces[i];
}

static FontSelectionRequest computeFontSelectionRequest(CSSPropertyParserHelpers::FontRaw& font)
{
    auto weightSelectionValue = font.weight
        ? WTF::switchOn(*font.weight, [&] (CSSValueID keyword) {
            switch (keyword) {
            case CSSValueNormal:
                return normalWeightValue();
            case CSSValueBold:
            case CSSValueBolder:
                return boldWeightValue();
            case CSSValueLighter:
                return lightWeightValue();
            default:
                ASSERT_NOT_REACHED();
                return normalWeightValue();
            }
        }, [&] (double weight) {
            return FontSelectionValue::clampFloat(weight);
        }) : normalWeightValue();

    // Because this is a FontRaw, we know we should be able to dereference stretchSelectionValue as
    // consumeFontStretchKeywordValueRaw only returns results valid to pass to fontStretchValue.
    auto stretchSelectionValue = fontStretchValue(font.stretch.value_or(CSSValueNormal));
    ASSERT(stretchSelectionValue);

    auto styleKeyword = font.style ? font.style->style : CSSValueNormal;
    auto styleSelectionValue = [&] () -> std::optional<FontSelectionValue> {
        if (styleKeyword == CSSValueNormal)
            return std::nullopt;
        if (styleKeyword == CSSValueItalic)
            return italicValue();
        ASSERT(font.style && styleKeyword == CSSValueOblique);
        float degrees = 0;
        if (font.style->angle)
            degrees = static_cast<float>(CSSPrimitiveValue::computeDegrees(font.style->angle->type, font.style->angle->value));
        return FontSelectionValue(degrees);
    }();

    return { weightSelectionValue, *stretchSelectionValue, styleSelectionValue };
}

using CodePointsMap = HashSet<UChar32, DefaultHash<UChar32>, WTF::UnsignedWithZeroKeyHashTraits<UChar32>>;
static CodePointsMap codePointsFromString(StringView stringView)
{
    CodePointsMap result;
    auto graphemeClusters = stringView.graphemeClusters();
    for (auto cluster : graphemeClusters) {
        ASSERT(cluster.length() > 0);
        UChar32 character = 0;
        if (cluster.is8Bit())
            character = cluster[0];
        else
            U16_GET(cluster.characters16(), 0, 0, cluster.length(), character);
        result.add(character);
    }
    return result;
}

ExceptionOr<Vector<std::reference_wrapper<CSSFontFace>>> CSSFontFaceSet::matchingFacesExcludingPreinstalledFonts(const String& fontShorthand, const String& string)
{
    auto font = CSSPropertyParserWorkerSafe::parseFont(fontShorthand, HTMLStandardMode);
    if (!font)
        return Exception { SyntaxError };

    HashSet<AtomString> uniqueFamilies;
    Vector<AtomString> familyOrder;
    for (auto& familyRaw : font->family) {
        AtomString familyAtom;
        WTF::switchOn(familyRaw, [&] (CSSValueID familyKeyword) {
            if (familyKeyword != CSSValueWebkitBody)
                familyAtom = familyNamesData->at(CSSPropertyParserHelpers::genericFontFamilyIndex(familyKeyword));
            else {
                ASSERT(m_owningFontSelector && m_owningFontSelector->scriptExecutionContext());
                familyAtom = AtomString { m_owningFontSelector->scriptExecutionContext()->settingsValues().fontGenericFamilies.standardFontFamily() };
            }
        }, [&] (const AtomString& familyString) {
            familyAtom = familyString;
        });

        if (!familyAtom.isEmpty() && uniqueFamilies.add(familyAtom).isNewEntry)
            familyOrder.append(familyAtom);
    }

    HashSet<CSSFontFace*> resultConstituents;
    auto request = computeFontSelectionRequest(*font);
    for (auto codePoint : codePointsFromString(string)) {
        bool found = false;
        for (auto& family : familyOrder) {
            auto* faces = fontFace(request, family);
            if (!faces)
                continue;
            for (auto& constituentFace : faces->constituentFaces()) {
                if (constituentFace->isLocalFallback())
                    continue;
                if (constituentFace->rangesMatchCodePoint(codePoint)) {
                    resultConstituents.add(constituentFace.ptr());
                    found = true;
                    break;
                }
            }
            if (found)
                break;
        }
    }

    return WTF::map(resultConstituents, [](auto* constituent) -> std::reference_wrapper<CSSFontFace> {
        return *constituent;
    });
}

ExceptionOr<bool> CSSFontFaceSet::check(const String& font, const String& text)
{
    auto matchingFaces = this->matchingFacesExcludingPreinstalledFonts(font, text);
    if (matchingFaces.hasException())
        return matchingFaces.releaseException();

    for (auto& face : matchingFaces.releaseReturnValue()) {
        if (face.get().status() == CSSFontFace::Status::Pending
            || face.get().status() == CSSFontFace::Status::Loading)
            return false;
    }
    return true;
}

CSSSegmentedFontFace* CSSFontFaceSet::fontFace(FontSelectionRequest request, const AtomString& family)
{
    auto iterator = m_facesLookupTable.find(family);
    if (iterator == m_facesLookupTable.end())
        return nullptr;
    auto& familyFontFaces = iterator->value;

    auto& segmentedFontFaceCache = m_cache.add(family, FontSelectionHashMap()).iterator->value;

    auto& face = segmentedFontFaceCache.add(request, nullptr).iterator->value;
    if (face)
        return face.get();

    face = CSSSegmentedFontFace::create();

    Vector<std::reference_wrapper<CSSFontFace>, 32> candidateFontFaces;
    for (int i = familyFontFaces.size() - 1; i >= 0; --i) {
        CSSFontFace& candidate = familyFontFaces[i];
        if (candidate.status() == CSSFontFace::Status::Failure)
            continue;
        if (!isItalic(request.slope) && isItalic(candidate.fontSelectionCapabilities().slope.minimum))
            continue;
        candidateFontFaces.append(candidate);
    }

    auto localIterator = m_locallyInstalledFacesLookupTable.find(family);
    if (localIterator != m_locallyInstalledFacesLookupTable.end()) {
        for (auto& candidate : localIterator->value) {
            if (candidate->status() == CSSFontFace::Status::Failure)
                continue;
            if (!isItalic(request.slope) && isItalic(candidate->fontSelectionCapabilities().slope.minimum))
                continue;
            candidateFontFaces.append(candidate);
        }
    }

    if (!candidateFontFaces.isEmpty()) {
        auto capabilities = candidateFontFaces.map([](auto& face) {
            return face.get().fontSelectionCapabilities();
        });
        FontSelectionAlgorithm fontSelectionAlgorithm(request, capabilities);
        std::stable_sort(candidateFontFaces.begin(), candidateFontFaces.end(), [&fontSelectionAlgorithm](const CSSFontFace& first, const CSSFontFace& second) {
            auto firstCapabilities = first.fontSelectionCapabilities();
            auto secondCapabilities = second.fontSelectionCapabilities();
            
            auto stretchDistanceFirst = fontSelectionAlgorithm.stretchDistance(firstCapabilities).distance;
            auto stretchDistanceSecond = fontSelectionAlgorithm.stretchDistance(secondCapabilities).distance;
            if (stretchDistanceFirst < stretchDistanceSecond)
                return true;
            if (stretchDistanceFirst > stretchDistanceSecond)
                return false;

            auto styleDistanceFirst = fontSelectionAlgorithm.styleDistance(firstCapabilities).distance;
            auto styleDistanceSecond = fontSelectionAlgorithm.styleDistance(secondCapabilities).distance;
            if (styleDistanceFirst < styleDistanceSecond)
                return true;
            if (styleDistanceFirst > styleDistanceSecond)
                return false;

            auto weightDistanceFirst = fontSelectionAlgorithm.weightDistance(firstCapabilities).distance;
            auto weightDistanceSecond = fontSelectionAlgorithm.weightDistance(secondCapabilities).distance;
            if (weightDistanceFirst < weightDistanceSecond)
                return true;
            return false;
        });
        CSSFontFace* previousCandidate = nullptr;
        for (auto& candidate : candidateFontFaces) {
            if (&candidate.get() == previousCandidate)
                continue;
            previousCandidate = &candidate.get();
            face->appendFontFace(candidate.get());
        }
    }

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
        m_fontEventClients.forEach([&] (auto& client) {
            client.faceFinished(face, newState);
        });
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

    m_fontModifiedObservers.forEach([] (auto& observer) {
        observer();
    });
}

}
