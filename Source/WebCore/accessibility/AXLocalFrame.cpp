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
#include "AXLocalFrame.h"

#include "AccessibilityObjectInlines.h"
#include "AccessibilityScrollView.h"
#include "LocalFrameInlines.h"

namespace WebCore {

AXLocalFrame::AXLocalFrame(AXID axID, AXObjectCache& cache)
    : AccessibilityMockObject(axID, cache)
{
}

Ref<AXLocalFrame> AXLocalFrame::create(AXID axID, AXObjectCache& cache)
{
    return adoptRef(*new AXLocalFrame(axID, cache));
}

LayoutRect AXLocalFrame::elementRect() const
{
    RefPtr parent = parentObject();
    return parent ? parent->elementRect() : LayoutRect();
}


bool AXLocalFrame::computeIsIgnored() const
{
#if ENABLE(ACCESSIBILITY_LOCAL_FRAME)
    if (RefPtr hostingScrollView = dynamicDowncast<AccessibilityScrollView>(parentObject()))
        return hostingScrollView->isIgnoredFromHostingFrame();
#endif
    return false;
}


#if ENABLE(ACCESSIBILITY_LOCAL_FRAME)

void AXLocalFrame::setLocalFrameView(LocalFrameView* localFrameView)
{
    m_localFrameView = localFrameView;
    m_frameID = localFrameView->frame().frameID();
}

AccessibilityObject* AXLocalFrame::crossFrameChildObject() const
{
    if (!m_localFrameView)
        return nullptr;

    RefPtr localFrame = m_localFrameView->frame();
    if (!localFrame)
        return nullptr;

    RefPtr document = localFrame->document();
    if (!document)
        return nullptr;

    CheckedPtr cache = document->axObjectCache();
    if (!cache)
        return nullptr;

    return downcast<AccessibilityObject>(cache->rootObjectForFrame(*localFrame.get()));
}

#endif // ENABLE(ACCESSIBILITY_LOCAL_FRAME)

} // namespace WebCore
