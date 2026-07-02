/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CSSPosition.h"
#include "CSSValue.h"

namespace WebCore {

class CSSPositionValue final : public CSSValue {
public:
    static Ref<CSSPositionValue> create(CSS::Position&&);

    const CSS::Position& position() const LIFETIME_BOUND { return m_position; }

    String customCSSText(const CSS::SerializationContext&) const;
    bool equals(const CSSPositionValue&) const;

    IterationStatus customVisitChildren(NOESCAPE const Function<IterationStatus(CSSValue&)>&) const;

private:
    CSSPositionValue(CSS::Position&&);

    CSS::Position m_position;
};

class CSSPositionXValue final : public CSSValue {
public:
    static Ref<CSSPositionXValue> create(CSS::PositionX&&);

    const CSS::PositionX& position() const LIFETIME_BOUND { return m_position; }

    String customCSSText(const CSS::SerializationContext&) const;
    bool equals(const CSSPositionXValue&) const;

    IterationStatus customVisitChildren(NOESCAPE const Function<IterationStatus(CSSValue&)>&) const;

private:
    CSSPositionXValue(CSS::PositionX&&);

    CSS::PositionX m_position;
};

class CSSPositionYValue final : public CSSValue {
public:
    static Ref<CSSPositionYValue> create(CSS::PositionY&&);

    const CSS::PositionY& position() const LIFETIME_BOUND { return m_position; }

    String customCSSText(const CSS::SerializationContext&) const;
    bool equals(const CSSPositionYValue&) const;

    IterationStatus customVisitChildren(NOESCAPE const Function<IterationStatus(CSSValue&)>&) const;

private:
    CSSPositionYValue(CSS::PositionY&&);

    CSS::PositionY m_position;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSPositionValue, isPositionValue())
SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSPositionXValue, isPositionXValue())
SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSPositionYValue, isPositionYValue())
