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

#include "CSSFontFamily.h"
#include "CSSFontSelector.h"
#include "CSSParser.h"
#include "CSSPrimitiveValue.h"
#include "CSSValueList.h"
#include "StyleProperties.h"

namespace WebCore {

CSSFontFaceSet::CSSFontFaceSet(CSSFontFaceSetClient& client)
    : m_client(client)
{
}

CSSFontFaceSet::~CSSFontFaceSet()
{
    for (auto& face : m_faces)
        face->removeClient(*this);
}

void CSSFontFaceSet::incrementActiveCount()
{
    ++m_activeCount;
    if (m_activeCount == 1) {
        m_status = Status::Loading;
        m_client.startedLoading();
    }
}

void CSSFontFaceSet::decrementActiveCount()
{
    --m_activeCount;
    if (!m_activeCount) {
        m_status = Status::Loaded;
        m_client.completedLoading();
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

void CSSFontFaceSet::add(CSSFontFace& face)
{
    ASSERT(!hasFace(face));

    m_faces.append(face);
    face.addClient(*this);
    if (face.status() == CSSFontFace::Status::Loading || face.status() == CSSFontFace::Status::TimedOut)
        incrementActiveCount();
}

void CSSFontFaceSet::remove(const CSSFontFace& face)
{
    for (size_t i = 0; i < m_faces.size(); ++i) {
        if (m_faces[i].ptr() == &face) {
            m_faces[i]->removeClient(*this);
            m_faces.remove(i);
            if (face.status() == CSSFontFace::Status::Loading || face.status() == CSSFontFace::Status::TimedOut)
                decrementActiveCount();
            return;
        }
    }
    ASSERT_NOT_REACHED();
}

static HashSet<String> extractFamilies(const CSSValueList& list)
{
    HashSet<String> result;
    for (auto& family : list) {
        const CSSPrimitiveValue& primitive = downcast<CSSPrimitiveValue>(family.get());
        if (!primitive.isFontFamily())
            continue;
        result.add(primitive.fontFamily().familyName);
    }
    return result;
}

static bool familiesIntersect(const CSSFontFace& face, const CSSValueList& request)
{
    if (!face.families())
        return false;

    HashSet<String> faceFamilies = extractFamilies(*face.families());
    HashSet<String> requestFamilies = extractFamilies(request);
    for (auto& family1 : faceFamilies) {
        if (requestFamilies.contains(family1))
            return true;
    }
    return false;
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
    bool desiredStyleIsNormal = true;
    if (RefPtr<CSSValue> desiredStyle = style->getPropertyCSSValue(CSSPropertyFontStyle)) {
        if (!is<CSSPrimitiveValue>(*desiredStyle)) {
            ec = SYNTAX_ERR;
            return result;
        }
        desiredStyleIsNormal = downcast<CSSPrimitiveValue>(*desiredStyle).getValueID() == CSSValueNormal;
    }
    RefPtr<CSSValue> family = style->getPropertyCSSValue(CSSPropertyFontFamily);
    if (!is<CSSValueList>(family.get())) {
        ec = SYNTAX_ERR;
        return result;
    }
    CSSValueList& familyList = downcast<CSSValueList>(*family);

    // Match CSSFontSelector::getFontFace()
    for (auto& face : m_faces) {
        if (!familiesIntersect(face, familyList) || (desiredStyleIsNormal && !(face->traitsMask() & FontStyleNormalMask)))
            continue;
        result.append(face.get());
    }
    return result;
}

void CSSFontFaceSet::load(const String& font, const String& text, ExceptionCode& ec)
{
    auto matchingFaces = this->matchingFaces(font, text, ec);
    if (ec)
        return;

    for (auto& face : matchingFaces)
        face.get().load();
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

void CSSFontFaceSet::stateChanged(CSSFontFace& face, CSSFontFace::Status oldState, CSSFontFace::Status newState)
{
    ASSERT(hasFace(face));
    if (oldState == CSSFontFace::Status::Pending) {
        ASSERT(newState == CSSFontFace::Status::Loading);
        incrementActiveCount();
    }
    if (newState == CSSFontFace::Status::Success || newState == CSSFontFace::Status::Failure) {
        ASSERT(oldState == CSSFontFace::Status::Loading || oldState == CSSFontFace::Status::TimedOut);
        m_client.faceFinished(face, newState);
        decrementActiveCount();
    }
}

}
