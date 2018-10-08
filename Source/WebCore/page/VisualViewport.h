/*
 * Copyright (C) 2017 Google Inc. All rights reserved.
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

#include "DOMWindowProperty.h"
#include "EventTarget.h"
#include <wtf/RefCounted.h>

namespace WebCore {

class VisualViewport final : public RefCounted<VisualViewport>, public EventTargetWithInlineData, public DOMWindowProperty {
public:
    static Ref<VisualViewport> create(DOMWindow& window) { return adoptRef(*new VisualViewport(window)); }

    // EventTarget
    EventTargetInterface eventTargetInterface() const final;
    ScriptExecutionContext* scriptExecutionContext() const final;
    bool addEventListener(const AtomicString& eventType, Ref<EventListener>&&, const AddEventListenerOptions&) final;

    double offsetLeft() const;
    double offsetTop() const;
    double pageLeft() const;
    double pageTop() const;
    double width() const;
    double height() const;
    double scale() const;

    void update();

    using RefCounted::ref;
    using RefCounted::deref;

private:
    explicit VisualViewport(DOMWindow&);

    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }

    void enqueueResizeEvent();
    void enqueueScrollEvent();

    void updateFrameLayout() const;

    double m_offsetLeft { 0 };
    double m_offsetTop { 0 };
    double m_pageLeft { 0 };
    double m_pageTop { 0 };
    double m_width { 0 };
    double m_height { 0 };
    double m_scale { 1 };
};

} // namespace WebCore
