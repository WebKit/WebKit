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
#include "CSSSegmentedFontFace.h"
#include "CSSValueList.h"
#include "CSSValuePool.h"
#include "FontCache.h"
#include "StyleBuilderConverter.h"
#include "StyleProperties.h"

namespace WebCore {

CSSFontFaceSet::CSSFontFaceSet(CSSFontSelector* owningFontSelector)
    : m_owningFontSelector(makeWeakPtr(owningFontSelector))
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

void CSSFontFaceSet::ensureLocalFontFacesForFamilyRegistered(const String& familyName)
{
    ASSERT(m_owningFontSelector);
    if (m_locallyInstalledFacesLookupTable.contains(familyName))
        return;

    AllowUserInstalledFonts allowUserInstalledFonts = AllowUserInstalledFonts::Yes;
    if (m_owningFontSelector->document())
        allowUserInstalledFonts = m_owningFontSelector->document()->settings().shouldAllowUserInstalledFonts() ? AllowUserInstalledFonts::Yes : AllowUserInstalledFonts::No;
    Vector<FontSelectionCapabilities> capabilities = FontCache::singleton().getFontSelectionCapabilitiesInFamily(familyName, allowUserInstalledFonts);
    if (capabilities.isEmpty())
        return;

    Vector<Ref<CSSFontFace>> faces;
    for (auto item : capabilities) {
        Ref<CSSFontFace> face = CSSFontFace::create(m_owningFontSelector.get(), nullptr, nullptr, true);
        
        Ref<CSSValueList> familyList = CSSValueList::createCommaSeparated();
        familyList->append(CSSValuePool::singleton().createFontFamilyValue(familyName));
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
    if (!face.families() || !face.families().hasValue())
        return;
    auto families = face.families().value();

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
    auto protect = makeRef(face);

    m_cache.clear();

    for (auto* client : m_clients)
        client->fontModified();
    
    if (face.families() && face.families().hasValue())
        removeFromFacesLookupTable(face, *face.families().value());

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

static ExceptionOr<FontSelectionRequest> computeFontSelectionRequest(MutableStyleProperties& style)
{
    RefPtr<CSSValue> weightValue = style.getPropertyCSSValue(CSSPropertyFontWeight).get();
    if (!weightValue || weightValue->isInitialValue())
        weightValue = CSSValuePool::singleton().createIdentifierValue(CSSValueNormal).ptr();

    RefPtr<CSSValue> stretchValue = style.getPropertyCSSValue(CSSPropertyFontStretch).get();
    if (!stretchValue || stretchValue->isInitialValue())
        stretchValue = CSSValuePool::singleton().createIdentifierValue(CSSValueNormal).ptr();

    RefPtr<CSSValue> styleValue = style.getPropertyCSSValue(CSSPropertyFontStyle).get();
    if (!styleValue || styleValue->isInitialValue())
        styleValue = CSSFontStyleValue::create(CSSValuePool::singleton().createIdentifierValue(CSSValueNormal));

    if (weightValue->isGlobalKeyword() || stretchValue->isGlobalKeyword() || styleValue->isGlobalKeyword())
        return Exception { SyntaxError };

    auto weightSelectionValue = Style::BuilderConverter::convertFontWeightFromValue(*weightValue);
    auto stretchSelectionValue = Style::BuilderConverter::convertFontStretchFromValue(*stretchValue);
    auto styleSelectionValue = Style::BuilderConverter::convertFontStyleFromValue(*styleValue);

    return {{ weightSelectionValue, stretchSelectionValue, styleSelectionValue }};
}

static HashSet<UChar32> codePointsFromString(StringView stringView)
{
    HashSet<UChar32> result;
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

ExceptionOr<Vector<std::reference_wrapper<CSSFontFace>>> CSSFontFaceSet::matchingFacesExcludingPreinstalledFonts(const String& font, const String& string)
{
    auto style = MutableStyleProperties::create();
    auto parseResult = CSSParser::parseValue(style, CSSPropertyFont, font, true, HTMLStandardMode);
    if (parseResult == CSSParser::ParseResult::Error)
        return Exception { SyntaxError };

    auto requestOrException = computeFontSelectionRequest(style.get());
    if (requestOrException.hasException())
        return requestOrException.releaseException();
    auto request = requestOrException.releaseReturnValue();

    auto family = style->getPropertyCSSValue(CSSPropertyFontFamily);
    if (!is<CSSValueList>(family))
        return Exception { SyntaxError };
    CSSValueList& familyList = downcast<CSSValueList>(*family);

    HashSet<AtomString> uniqueFamilies;
    Vector<AtomString> familyOrder;
    for (auto& family : familyList) {
        auto& primitive = downcast<CSSPrimitiveValue>(family.get());
        if (!primitive.isFontFamily())
            continue;
        if (uniqueFamilies.add(primitive.fontFamily().familyName).isNewEntry)
            familyOrder.append(primitive.fontFamily().familyName);
    }

    HashSet<CSSFontFace*> resultConstituents;
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

    Vector<std::reference_wrapper<CSSFontFace>> result;
    result.reserveInitialCapacity(resultConstituents.size());
    for (auto* constituent : resultConstituents)
        result.uncheckedAppend(*constituent);
    return result;
}

ExceptionOr<bool> CSSFontFaceSet::check(const String& font, const String& text)
{
    auto matchingFaces = this->matchingFacesExcludingPreinstalledFonts(font, text);
    if (matchingFaces.hasException())
        return matchingFaces.releaseException();

    for (auto& face : matchingFaces.releaseReturnValue()) {
        if (face.get().status() == CSSFontFace::Status::Pending)
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
        auto capabilitiesWrapped = candidate.fontSelectionCapabilities();
        if (!capabilitiesWrapped.hasValue())
            continue;
        auto capabilities = capabilitiesWrapped.value();
        if (!isItalic(request.slope) && isItalic(capabilities.slope.minimum))
            continue;
        candidateFontFaces.append(candidate);
    }

    auto localIterator = m_locallyInstalledFacesLookupTable.find(family);
    if (localIterator != m_locallyInstalledFacesLookupTable.end()) {
        for (auto& candidate : localIterator->value) {
            auto capabilitiesWrapped = candidate->fontSelectionCapabilities();
            if (!capabilitiesWrapped.hasValue())
                continue;
            auto capabilities = capabilitiesWrapped.value();
            if (!isItalic(request.slope) && isItalic(capabilities.slope.minimum))
                continue;
            candidateFontFaces.append(candidate);
        }
    }

    if (!candidateFontFaces.isEmpty()) {
        Vector<FontSelectionCapabilities> capabilities;
        capabilities.reserveInitialCapacity(candidateFontFaces.size());
        for (auto& face : candidateFontFaces) {
            auto fontSelectionCapabilitiesWrapped = face.get().fontSelectionCapabilities();
            ASSERT(fontSelectionCapabilitiesWrapped.hasValue());
            auto fontSelectionCapabilities = fontSelectionCapabilitiesWrapped.value();
            capabilities.uncheckedAppend(fontSelectionCapabilities);
        }
        FontSelectionAlgorithm fontSelectionAlgorithm(request, capabilities);
        std::stable_sort(candidateFontFaces.begin(), candidateFontFaces.end(), [&fontSelectionAlgorithm](const CSSFontFace& first, const CSSFontFace& second) {
            auto firstCapabilitiesWrapped = first.fontSelectionCapabilities();
            auto secondCapabilitiesWrapped = second.fontSelectionCapabilities();
            ASSERT(firstCapabilitiesWrapped.hasValue() && secondCapabilitiesWrapped.hasValue());
            
            auto firstCapabilities = firstCapabilitiesWrapped.value();
            auto secondCapabilities = secondCapabilitiesWrapped.value();
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
        for (auto& candidate : candidateFontFaces)
            face->appendFontFace(candidate.get());
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
