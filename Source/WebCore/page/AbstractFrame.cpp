/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "AbstractFrame.h"

#include "DocumentInlines.h"
#include "HTMLFrameOwnerElement.h"
#include "Page.h"
#include "WindowProxy.h"

namespace WebCore {

AbstractFrame::AbstractFrame(Page& page, FrameIdentifier frameID, AbstractFrame* parent)
    : m_page(page)
    , m_frameID(frameID)
    , m_treeNode(*this, parent)
    , m_windowProxy(WindowProxy::create(*this))
{
    if (parent)
        parent->tree().appendChild(*this);
}

AbstractFrame::~AbstractFrame()
{
    m_windowProxy->detachFromFrame();
}

void AbstractFrame::resetWindowProxy()
{
    m_windowProxy->detachFromFrame();
    m_windowProxy = WindowProxy::create(*this);
}

Page* AbstractFrame::page() const
{
    return m_page.get();
}

void AbstractFrame::detachFromPage()
{
    m_page = nullptr;
}

} // namespace WebCore
