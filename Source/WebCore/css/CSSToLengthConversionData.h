/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/CSSPropertyNames.h>
#include <WebCore/Element.h>

namespace WebCore {

class FontCascade;
class RenderView;

namespace Style {
class BuilderState;
class ComputedStyle;
}

class CSSToLengthConversionData {
public:
    CSSToLengthConversionData(const CSSToLengthConversionData&);
    CSSToLengthConversionData(CSSToLengthConversionData&&);

    explicit CSSToLengthConversionData(const Style::ComputedStyle&, Style::BuilderState&);
    explicit CSSToLengthConversionData(const Style::ComputedStyle&, const Style::ComputedStyle* rootStyle, const Style::ComputedStyle* parentStyle, const RenderView*, const Element* elementForContainerUnitResolution);

    WEBCORE_EXPORT ~CSSToLengthConversionData();

    const Style::ComputedStyle& style() const { return m_style; }
    const Style::ComputedStyle* rootStyle() const { return m_rootStyle; }
    const Style::ComputedStyle* parentStyle() const { return m_parentStyle; }
    const RenderView* renderView() const { return m_renderView; }
    const Element* elementForContainerUnitResolution() const { return m_elementForContainerUnitResolution.get(); }
    CSSPropertyID property() const { return m_property; }
    Style::BuilderState* styleBuilderState() const { return m_styleBuilderState.get(); }

private:
    friend class Style::BuilderState;

    const Style::ComputedStyle& m_style;
    const Style::ComputedStyle* m_rootStyle { nullptr };
    const Style::ComputedStyle* m_parentStyle { nullptr };
    const RenderView* m_renderView { nullptr };
    RefPtr<const Element> m_elementForContainerUnitResolution;
    CSSPropertyID m_property { CSSPropertyInvalid };
    CheckedPtr<Style::BuilderState> m_styleBuilderState;
};

} // namespace WebCore
