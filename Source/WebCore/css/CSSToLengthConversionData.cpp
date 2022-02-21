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

namespace WebCore {

float CSSToLengthConversionData::zoom() const
{
    if (!m_zoom)
        return m_style ? m_style->effectiveZoom() : 1;
    return *m_zoom;
}

FloatSize CSSToLengthConversionData::defaultViewportFactor() const
{
    if (m_viewportDependencyDetectionStyle)
        m_viewportDependencyDetectionStyle->setHasViewportUnits();

    if (!m_renderView)
        return { };

    return m_renderView->sizeForCSSDefaultViewportUnits() / 100.0;
}

FloatSize CSSToLengthConversionData::smallViewportFactor() const
{
    if (m_viewportDependencyDetectionStyle)
        m_viewportDependencyDetectionStyle->setHasViewportUnits();

    if (!m_renderView)
        return { };

    return m_renderView->sizeForCSSSmallViewportUnits() / 100.0;
}

FloatSize CSSToLengthConversionData::largeViewportFactor() const
{
    if (m_viewportDependencyDetectionStyle)
        m_viewportDependencyDetectionStyle->setHasViewportUnits();

    if (!m_renderView)
        return { };

    return m_renderView->sizeForCSSLargeViewportUnits() / 100.0;
}

FloatSize CSSToLengthConversionData::dynamicViewportFactor() const
{
    if (m_viewportDependencyDetectionStyle)
        m_viewportDependencyDetectionStyle->setHasViewportUnits();

    if (!m_renderView)
        return { };

    return m_renderView->sizeForCSSDynamicViewportUnits() / 100.0;
}

} // namespace WebCore
