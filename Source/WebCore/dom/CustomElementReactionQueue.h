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

#include "GCReachableRef.h"
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>

namespace JSC {

class ExecState;

}

namespace WebCore {

class CustomElementReactionQueueItem;
class Document;
class Element;
class JSCustomElementInterface;
class QualifiedName;

class CustomElementReactionQueue {
    WTF_MAKE_NONCOPYABLE(CustomElementReactionQueue);
public:
    CustomElementReactionQueue(JSCustomElementInterface&);
    ~CustomElementReactionQueue();

    static void enqueueElementUpgrade(Element&, bool alreadyScheduledToUpgrade);
    static void enqueueElementUpgradeIfDefined(Element&);
    static void enqueueConnectedCallbackIfNeeded(Element&);
    static void enqueueDisconnectedCallbackIfNeeded(Element&);
    static void enqueueAdoptedCallbackIfNeeded(Element&, Document& oldDocument, Document& newDocument);
    static void enqueueAttributeChangedCallbackIfNeeded(Element&, const QualifiedName&, const AtomicString& oldValue, const AtomicString& newValue);
    static void enqueuePostUpgradeReactions(Element&);

    bool observesStyleAttribute() const;
    void invokeAll(Element&);
    void clear();

    static void processBackupQueue();

    class ElementQueue {
    public:
        void add(Element&);
        void processQueue(JSC::ExecState*);

    private:
        void invokeAll();

        Vector<GCReachableRef<Element>> m_elements;
        bool m_invoking { false };
    };

private:
    static void enqueueElementOnAppropriateElementQueue(Element&);
    static ElementQueue& ensureBackupQueue();
    static ElementQueue& backupElementQueue();

    Ref<JSCustomElementInterface> m_interface;
    Vector<CustomElementReactionQueueItem> m_items;
};

class CustomElementReactionDisallowedScope {
public:
    CustomElementReactionDisallowedScope()
    {
#if !ASSERT_DISABLED
        s_customElementReactionDisallowedCount++;
#endif
    }

    ~CustomElementReactionDisallowedScope()
    {
#if !ASSERT_DISABLED
        ASSERT(s_customElementReactionDisallowedCount);
        s_customElementReactionDisallowedCount--;
#endif
    }

#if !ASSERT_DISABLED
    static bool isReactionAllowed() { return !s_customElementReactionDisallowedCount; }
#endif

    class AllowedScope {
#if !ASSERT_DISABLED
    public:
        AllowedScope()
            : m_originalCount(s_customElementReactionDisallowedCount)
        {
            s_customElementReactionDisallowedCount = 0;
        }

        ~AllowedScope()
        {
            s_customElementReactionDisallowedCount = m_originalCount;
        }

    private:
        unsigned m_originalCount;
#endif
    };

private:
#if !ASSERT_DISABLED
    WEBCORE_EXPORT static unsigned s_customElementReactionDisallowedCount;

    friend class AllowedScope;
#endif
};

class CustomElementReactionStack : public CustomElementReactionDisallowedScope::AllowedScope {
public:
    ALWAYS_INLINE CustomElementReactionStack(JSC::ExecState* state)
        : m_previousProcessingStack(s_currentProcessingStack)
        , m_state(state)
    {
        s_currentProcessingStack = this;
    }

    ALWAYS_INLINE CustomElementReactionStack(JSC::ExecState& state)
        : CustomElementReactionStack(&state)
    { }

    ALWAYS_INLINE ~CustomElementReactionStack()
    {
        if (UNLIKELY(m_queue))
            processQueue(m_state);
        s_currentProcessingStack = m_previousProcessingStack;
    }

private:
    WEBCORE_EXPORT void processQueue(JSC::ExecState*);

    CustomElementReactionQueue::ElementQueue* m_queue { nullptr }; // Use raw pointer to avoid generating delete in the destructor.
    CustomElementReactionStack* m_previousProcessingStack;
    JSC::ExecState* m_state;

    WEBCORE_EXPORT static CustomElementReactionStack* s_currentProcessingStack;

    friend CustomElementReactionQueue;
};

}
