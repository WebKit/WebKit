/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "DisplayView.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "Frame.h"
#include "FrameView.h"
#include "FrameViewLayoutContext.h"
#include "LayoutContext.h"
#include "LayoutState.h"
#include "Page.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {
namespace Display {

WTF_MAKE_ISO_ALLOCATED_IMPL(View);

View::View(FrameView& frameView)
    : m_frameView(frameView)
    , m_layerController(*this)
{
}

View::~View()
{
}

Frame& View::frame() const
{
    return m_frameView.frame();
}

Page* View::page() const
{
    return m_frameView.frame().page();
}

const Layout::LayoutState* View::layoutState() const
{
    return m_frameView.layoutContext().layoutFormattingState();
}

void View::prepareForDisplay()
{
    auto* layoutState = this->layoutState();
    if (!layoutState)
        return;

    m_layerController.prepareForDisplay(*layoutState);
}

void View::flushLayers()
{
    m_layerController.flushLayers();
}



} // namespace Display
} // namespace WebCore

#endif // ENABLE(LAYOUT_FORMATTING_CONTEXT)
