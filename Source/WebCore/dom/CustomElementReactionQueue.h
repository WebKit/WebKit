/*
 * Copyright (C) 2015, 2016 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(CUSTOM_ELEMENTS)

#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>

namespace WebCore {

class CustomElementReactionQueueItem;
class Document;
class Element;
class JSCustomElementInterface;
class QualifiedName;

class CustomElementReactionQueue {
    WTF_MAKE_NONCOPYABLE(CustomElementReactionQueue);
public:
    CustomElementReactionQueue();
    ~CustomElementReactionQueue();

    static void enqueueElementUpgrade(Element&, JSCustomElementInterface&);
    static void enqueueConnectedCallbackIfNeeded(Element&);
    static void enqueueDisconnectedCallbackIfNeeded(Element&);
    static void enqueueAdoptedCallbackIfNeeded(Element&, Document& oldDocument, Document& newDocument);
    static void enqueueAttributeChangedCallbackIfNeeded(Element&, const QualifiedName&, const AtomicString& oldValue, const AtomicString& newValue);

    void invokeAll();

private:
    Vector<CustomElementReactionQueueItem> m_items;
};

class CustomElementReactionStack {
public:
    CustomElementReactionStack()
        : m_previousProcessingStack(s_currentProcessingStack)
    {
        s_currentProcessingStack = this;
    }

    ~CustomElementReactionStack()
    {
        if (UNLIKELY(m_queue))
            processQueue();
        s_currentProcessingStack = m_previousProcessingStack;
    }

    // FIXME: This should be a reference once "ensure" starts to work.
    static CustomElementReactionQueue* ensureCurrentQueue();

    static bool hasCurrentProcessingStack() { return s_currentProcessingStack; }

private:
    WEBCORE_EXPORT void processQueue();

    CustomElementReactionQueue* m_queue { nullptr };
    CustomElementReactionStack* m_previousProcessingStack;

    WEBCORE_EXPORT static CustomElementReactionStack* s_currentProcessingStack;
};

}

#endif
