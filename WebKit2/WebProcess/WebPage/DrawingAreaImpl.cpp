/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#include "DrawingAreaImpl.h"

#ifndef __APPLE__
#error "This drawing area is not ready for use by other ports yet."
#endif

using namespace WebCore;

namespace WebKit {

PassRefPtr<DrawingAreaImpl> DrawingAreaImpl::create(DrawingAreaInfo::Identifier identifier, WebPage* webPage)
{
    return adoptRef(new DrawingAreaImpl(identifier, webPage));
}

DrawingAreaImpl::~DrawingAreaImpl()
{
}

DrawingAreaImpl::DrawingAreaImpl(DrawingAreaInfo::Identifier identifier, WebPage* webPage)
    : DrawingArea(DrawingAreaInfo::Impl, identifier, webPage)
{
}

void DrawingAreaImpl::setNeedsDisplay(const IntRect&)
{
}

void DrawingAreaImpl::scroll(const IntSize& scrollDelta, const IntRect& rectToScroll, const IntRect& clipRect)
{
}

void DrawingAreaImpl::attachCompositingContext()
{
}

void DrawingAreaImpl::detachCompositingContext()
{
}

void DrawingAreaImpl::setRootCompositingLayer(WebCore::GraphicsLayer*)
{
}

void DrawingAreaImpl::scheduleCompositingLayerSync()
{
}

void DrawingAreaImpl::syncCompositingLayers()
{
}

void DrawingAreaImpl::didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*)
{
}
    
} // namespace WebKit
