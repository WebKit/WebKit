/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#include "Accessibility.h"

#include "AXObjectCache.h"
#include "AccessibilityObject.h"
#include "Frame.h"
#include "ScreenReader.h"

namespace WebCore {

Accessibility::Accessibility(Frame* frame)
    : m_frame(frame) 
{
}

Accessibility::~Accessibility()
{
    disconnectFrame();
}

void Accessibility::disconnectFrame()
{
    if (m_screenReader) {
        m_screenReader->disconnectFrame();
        m_screenReader = 0;
    }
    m_frame = 0;
}
    
void Accessibility::screenChanged()
{
    if (!m_frame)
        return;
    
    Document* doc = m_frame->document();
    if (!doc)
        return;
    
    AccessibilityObject* root = doc->axObjectCache()->getOrCreate(doc->renderer());
    doc->axObjectCache()->postNotification(root, doc, AXObjectCache::AXScreenChanged, false);
}

void Accessibility::elementsChanged()
{
    if (!m_frame)
        return;
    
    Document* doc = m_frame->document();
    if (!doc)
        return;
    
    AccessibilityObject* root = doc->axObjectCache()->getOrCreate(doc->renderer());
    doc->axObjectCache()->postNotification(root, doc, AXObjectCache::AXElementsChanged, false);
}
    
ScreenReader* Accessibility::screenReader() const
{
    if (!m_screenReader)
        m_screenReader = ScreenReader::create(m_frame);
    
    return m_screenReader.get();
}
    
} // namespace WebCore

