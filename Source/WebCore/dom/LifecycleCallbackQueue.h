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

#ifndef LifecycleCallbackQueue_h
#define LifecycleCallbackQueue_h

#if ENABLE(CUSTOM_ELEMENTS)

#include <wtf/Noncopyable.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomicString.h>

namespace WebCore {

class JSCustomElementInterface;
class Document;
class Element;
class QualifiedName;
class LifecycleQueueItem;

class LifecycleCallbackQueue {
    WTF_MAKE_NONCOPYABLE(LifecycleCallbackQueue);
public:
    LifecycleCallbackQueue();
    ~LifecycleCallbackQueue();

    static void enqueueAttributeChangedCallback(Element&, JSCustomElementInterface&,
        const QualifiedName&, const AtomicString& oldValue, const AtomicString& newValue);

    void invokeAll();

private:
    Vector<LifecycleQueueItem> m_items;
};

class CustomElementLifecycleProcessingStack {
public:
    CustomElementLifecycleProcessingStack()
        : m_previousProcessingStack(s_currentProcessingStack)
    {
        s_currentProcessingStack = this;
    }

    ~CustomElementLifecycleProcessingStack()
    {
        if (UNLIKELY(m_queue))
            processQueue();
        s_currentProcessingStack = m_previousProcessingStack;
    }

    // FIXME: This should be a reference once "ensure" starts to work.
    static LifecycleCallbackQueue* ensureCurrentQueue();

    static bool hasCurrentProcessingStack() { return s_currentProcessingStack; }

private:
    void processQueue();

    LifecycleCallbackQueue* m_queue { nullptr };
    CustomElementLifecycleProcessingStack* m_previousProcessingStack;

    static CustomElementLifecycleProcessingStack* s_currentProcessingStack;
};

}

#endif

#endif // LifecycleCallbackQueue_h
