/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "CanvasBase.h"

#include "CSSCanvasValue.h"
#include "CanvasRenderingContext.h"
#include "Element.h"
#include "FloatRect.h"
#include "InspectorInstrumentation.h"

namespace WebCore {

CanvasBase::CanvasBase(ScriptExecutionContext* scriptExecutionContext)
    : m_scriptExecutionContext(scriptExecutionContext)
{
}

CanvasBase::~CanvasBase()
{
    ASSERT(!m_context); // Should have been set to null by base class.
    notifyObserversCanvasDestroyed();
}

CanvasRenderingContext* CanvasBase::renderingContext() const
{
    return m_context.get();
}

void CanvasBase::addObserver(CanvasObserver& observer)
{
    m_observers.add(&observer);

    if (is<CSSCanvasValue::CanvasObserverProxy>(observer))
        InspectorInstrumentation::didChangeCSSCanvasClientNodes(*this);
}

void CanvasBase::removeObserver(CanvasObserver& observer)
{
    m_observers.remove(&observer);

    if (is<CSSCanvasValue::CanvasObserverProxy>(observer))
        InspectorInstrumentation::didChangeCSSCanvasClientNodes(*this);
}

void CanvasBase::notifyObserversCanvasChanged(const FloatRect& rect)
{
    for (auto& observer : m_observers)
        observer->canvasChanged(*this, rect);
}

void CanvasBase::notifyObserversCanvasResized()
{
    for (auto& observer : m_observers)
        observer->canvasResized(*this);
}

void CanvasBase::notifyObserversCanvasDestroyed()
{
    for (auto& observer : m_observers)
        observer->canvasDestroyed(*this);

    m_observers.clear();
}

HashSet<Element*> CanvasBase::cssCanvasClients() const
{
    HashSet<Element*> cssCanvasClients;
    for (auto& observer : m_observers) {
        if (!is<CSSCanvasValue::CanvasObserverProxy>(observer))
            continue;

        auto clients = downcast<CSSCanvasValue::CanvasObserverProxy>(observer)->ownerValue().clients();
        for (auto& entry : clients) {
            if (RefPtr<Element> element = entry.key->element())
                cssCanvasClients.add(element.get());
        }
    }
    return cssCanvasClients;
}

}
