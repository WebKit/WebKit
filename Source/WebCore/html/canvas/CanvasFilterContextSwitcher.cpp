/*
 * Copyright (C) 2024 Apple Inc.  All rights reserved.
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
#include "CanvasFilterContextSwitcher.h"

#include "CanvasLayerContextSwitcher.h"
#include "CanvasRenderingContext2DBase.h"
#include "FloatRect.h"

namespace WebCore {

std::unique_ptr<CanvasFilterContextSwitcher> CanvasFilterContextSwitcher::create(CanvasRenderingContext2DBase& context, const FloatRect& bounds)
{
    if (context.state().filterOperations.isEmpty())
        return nullptr;

    auto filter = context.createFilter(bounds);
    if (!filter)
        return nullptr;

    auto filterSwitcher = makeUnique<CanvasFilterContextSwitcher>(context);
    if (!filterSwitcher)
        return nullptr;

    auto targetSwitcher = CanvasLayerContextSwitcher::create(context, bounds, WTFMove(filter));
    if (!targetSwitcher)
        return nullptr;

    context.modifiableState().targetSwitcher = WTFMove(targetSwitcher);
    return filterSwitcher;
}

CanvasFilterContextSwitcher::CanvasFilterContextSwitcher(CanvasRenderingContext2DBase& context)
    : m_context(context)
{
    m_context.save();
    m_context.realizeSaves();
}

CanvasFilterContextSwitcher::~CanvasFilterContextSwitcher()
{
    m_context.restore();
}

FloatRect CanvasFilterContextSwitcher::expandedBounds() const
{
    return m_context.state().targetSwitcher->expandedBounds();
}

} // namespace WebCore
