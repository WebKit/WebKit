/*
 * Copyright (C) 2019 Igalia S.L.
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

#pragma once

#include "DOMRectReadOnly.h"
#include "Element.h"
#include "FloatRect.h"
#include "ResizeObserverSize.h"
#include <wtf/RefCounted.h>

namespace WebCore {

class Element;
class ResizeObserverSize;

class ResizeObserverEntry : public RefCounted<ResizeObserverEntry> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<ResizeObserverEntry> create(Element* target, const FloatRect& contentRect, FloatSize borderBoxSize, FloatSize contentBoxSize)
    {
        return adoptRef(*new ResizeObserverEntry(target, contentRect, borderBoxSize, contentBoxSize));
    }

    Element* target() const { return m_target.get(); }
    DOMRectReadOnly* contentRect() const { return m_contentRect.ptr(); }
    
    const Vector<Ref<ResizeObserverSize>>& borderBoxSize() const { return m_borderBoxSizes; }
    const Vector<Ref<ResizeObserverSize>>& contentBoxSize() const { return m_contentBoxSizes; }

private:
    ResizeObserverEntry(Element* target, const FloatRect& contentRect, FloatSize borderBoxSize, FloatSize contentBoxSize)
        : m_target(target)
        , m_contentRect(DOMRectReadOnly::create(contentRect.x(), contentRect.y(), contentRect.width(), contentRect.height()))
        , m_borderBoxSizes({ ResizeObserverSize::create(borderBoxSize.width(), borderBoxSize.height()) })
        , m_contentBoxSizes({ ResizeObserverSize::create(contentBoxSize.width(), contentBoxSize.height()) })
    {
    }

    RefPtr<Element> m_target;
    Ref<DOMRectReadOnly> m_contentRect;
    // The spec is designed to allow mulitple boxes for multicol scenarios, but for now these vectors only ever contain one entry.
    Vector<Ref<ResizeObserverSize>> m_borderBoxSizes;
    Vector<Ref<ResizeObserverSize>> m_contentBoxSizes;
};

} // namespace WebCore
