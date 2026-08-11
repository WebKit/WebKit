/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "LargestContentfulPaint.h"

#include "Element.h"
#include "LargestContentfulPaintData.h"

namespace WebCore {

LargestContentfulPaint::LargestContentfulPaint(DOMHighResTimeStamp timeStamp)
    : PerformanceEntry(emptyString(), timeStamp, timeStamp)
{
}

LargestContentfulPaint::~LargestContentfulPaint() = default;

DOMHighResTimeStamp LargestContentfulPaint::paintTime() const
{
    // https://github.com/w3c/largest-contentful-paint/issues/145
    return m_renderTime;
}

std::optional<DOMHighResTimeStamp> LargestContentfulPaint::presentationTime() const
{
    return { };
}

DOMHighResTimeStamp LargestContentfulPaint::loadTime() const
{
    return m_loadTime;
}

void LargestContentfulPaint::setLoadTime(DOMHighResTimeStamp loadTime)
{
    m_loadTime = loadTime;
}

DOMHighResTimeStamp LargestContentfulPaint::renderTime() const
{
    return m_renderTime;
}

void LargestContentfulPaint::setRenderTime(DOMHighResTimeStamp renderTime)
{
    m_renderTime = renderTime;
}

DOMHighResTimeStamp LargestContentfulPaint::startTime() const
{
    return m_renderTime ? m_renderTime : m_loadTime;
}

unsigned LargestContentfulPaint::size() const
{
    return m_pixelArea;
}

void LargestContentfulPaint::setSize(unsigned size)
{
    m_pixelArea = size;
}

String LargestContentfulPaint::id() const
{
    return m_id;
}

void LargestContentfulPaint::setID(const String& idString)
{
    m_id = idString;
}

String LargestContentfulPaint::url() const
{
    return m_urlString;
}

void LargestContentfulPaint::setURLString(const String& urlString)
{
    m_urlString = urlString;
}

Element* LargestContentfulPaint::element() const
{
    RefPtr element = m_element.get();
    if (!element)
        return nullptr;

    // The spec requires that the element accessor re-check connectedness.
    // https://w3c.github.io/largest-contentful-paint/#ref-for-dom-largestcontentfulpaint-element
    if (!LargestContentfulPaintData::isExposedForPaintTiming(*element))
        return nullptr;

    ASSERT(m_element == element.get());
    return m_element.get();
}

void LargestContentfulPaint::setElement(Element* element)
{
    m_element = element;
}

} // namespace WebCore
