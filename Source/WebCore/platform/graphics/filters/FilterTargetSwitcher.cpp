/*
 * Copyright (C) 2022 Apple Inc.  All rights reserved.
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
#include "FilterTargetSwitcher.h"

#include "Filter.h"
#include "FilterResults.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"

namespace WebCore {

FilterTargetSwitcher::FilterTargetSwitcher(Client& client, const DestinationColorSpace& colorSpace, const std::function<FloatRect()>& boundsProvider)
    : m_client(client)
{
    auto* context = m_client.drawingContext();
    if (!context)
        return;

    m_filter = m_client.createFilter([&]() {
        m_bounds = boundsProvider();
        return m_bounds;
    });

    if (!m_filter)
        return;

    ASSERT(!m_bounds.isEmpty());

    m_imageBuffer = context->createScaledImageBuffer(m_bounds, { 1, 1 }, colorSpace);
    if (!m_imageBuffer) {
        m_filter = nullptr;
        return;
    }

    auto state = context->state();
    m_imageBuffer->context().mergeAllChanges(state);
    m_client.setTargetSwitcher(this);
}

FilterTargetSwitcher::FilterTargetSwitcher(Client& client, const DestinationColorSpace& colorSpace, const FloatRect& bounds)
    : FilterTargetSwitcher(client, colorSpace, [&]() {
        return bounds;
    })
{
}

FilterTargetSwitcher::~FilterTargetSwitcher()
{
    if (!m_filter)
        return;

    ASSERT(m_imageBuffer);
    ASSERT(!m_bounds.isEmpty());

    m_client.setTargetSwitcher(nullptr);

    auto* context = m_client.drawingContext();
    FilterResults results;
    context->drawFilteredImageBuffer(m_imageBuffer.get(), m_bounds, *m_filter, results);
}

GraphicsContext* FilterTargetSwitcher::drawingContext() const
{
    if (m_imageBuffer)
        return &m_imageBuffer->context();
    return m_client.drawingContext();
}

FloatBoxExtent FilterTargetSwitcher::outsets() const
{
    if (!m_filter)
        return { };

    ASSERT(!m_bounds.isEmpty());

    auto outsets = m_client.calculateFilterOutsets(m_bounds);
    return { outsets.top(), outsets.right(), outsets.bottom(), outsets.left() };
}

} // namespace WebCore
