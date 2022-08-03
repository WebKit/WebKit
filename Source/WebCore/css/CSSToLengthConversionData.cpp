/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "config.h"
#include "CSSToLengthConversionData.h"

#include "FloatSize.h"
#include "RenderStyle.h"
#include "RenderView.h"
#include "StyleBuilderState.h"

namespace WebCore {

CSSToLengthConversionData::CSSToLengthConversionData(const RenderStyle& style, const Style::BuilderContext& builderContext)
    : m_style(&style)
    , m_rootStyle(builderContext.rootElementStyle)
    , m_parentStyle(&builderContext.parentStyle)
    , m_renderView(builderContext.document->renderView())
    , m_elementForContainerUnitResolution(builderContext.element)
    , m_viewportDependencyDetectionStyle(const_cast<RenderStyle*>(m_style))
{
}

CSSToLengthConversionData::CSSToLengthConversionData(const RenderStyle& style, const RenderStyle* rootStyle, const RenderStyle* parentStyle, const RenderView* renderView, const Element* elementForContainerUnitResolution)
    : m_style(&style)
    , m_rootStyle(rootStyle)
    , m_parentStyle(parentStyle)
    , m_renderView(renderView)
    , m_elementForContainerUnitResolution(elementForContainerUnitResolution)
    , m_zoom(1.f)
    , m_viewportDependencyDetectionStyle(const_cast<RenderStyle*>(m_style))
{
}

const FontCascade& CSSToLengthConversionData::fontCascadeForFontUnits() const
{
    if (computingFontSize()) {
        ASSERT(parentStyle());
        return parentStyle()->fontCascade();
    }
    ASSERT(style());
    return style()->fontCascade();
}

int CSSToLengthConversionData::computedLineHeightForFontUnits() const
{
    if (computingFontSize()) {
        ASSERT(parentStyle());
        return parentStyle()->computedLineHeight();
    }
    ASSERT(style());
    return style()->computedLineHeight();
}

float CSSToLengthConversionData::zoom() const
{
    return m_zoom.value_or(m_style ? m_style->effectiveZoom() : 1.f);
}

FloatSize CSSToLengthConversionData::defaultViewportFactor() const
{
    if (m_viewportDependencyDetectionStyle)
        m_viewportDependencyDetectionStyle->setUsesViewportUnits();

    if (!m_renderView)
        return { };

    return m_renderView->sizeForCSSDefaultViewportUnits() / 100.0;
}

FloatSize CSSToLengthConversionData::smallViewportFactor() const
{
    if (m_viewportDependencyDetectionStyle)
        m_viewportDependencyDetectionStyle->setUsesViewportUnits();

    if (!m_renderView)
        return { };

    return m_renderView->sizeForCSSSmallViewportUnits() / 100.0;
}

FloatSize CSSToLengthConversionData::largeViewportFactor() const
{
    if (m_viewportDependencyDetectionStyle)
        m_viewportDependencyDetectionStyle->setUsesViewportUnits();

    if (!m_renderView)
        return { };

    return m_renderView->sizeForCSSLargeViewportUnits() / 100.0;
}

FloatSize CSSToLengthConversionData::dynamicViewportFactor() const
{
    if (m_viewportDependencyDetectionStyle)
        m_viewportDependencyDetectionStyle->setUsesViewportUnits();

    if (!m_renderView)
        return { };

    return m_renderView->sizeForCSSDynamicViewportUnits() / 100.0;
}

void CSSToLengthConversionData::setUsesContainerUnits() const
{
    if (m_viewportDependencyDetectionStyle)
        m_viewportDependencyDetectionStyle->setUsesContainerUnits();
}

} // namespace WebCore
