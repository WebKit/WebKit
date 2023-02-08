/*
 * Copyright (C) 2015-2022 Apple Inc. All rights reserved.
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

#include "CustomElementFormValue.h"
#include "GCReachableRef.h"
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>

namespace JSC {

class JSGlobalObject;
class CallFrame;

}

namespace WebCore {

class CustomElementReactionQueueItem;
class Document;
class Element;
class HTMLFormElement;
class JSCustomElementInterface;
class QualifiedName;

// https://html.spec.whatwg.org/multipage/custom-elements.html#element-queue
class CustomElementQueue {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(CustomElementQueue);
public:
    CustomElementQueue() = default;

    void add(Element&);
    void processQueue(JSC::JSGlobalObject*);

    Vector<GCReachableRef<Element>> takeElements();

private:
    void invokeAll();

    Vector<GCReachableRef<Element>> m_elements;
    bool m_invoking { false };
};

class CustomElementReactionQueue {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(CustomElementReactionQueue);
public:
    CustomElementReactionQueue(JSCustomElementInterface&);
    ~CustomElementReactionQueue();

    static void enqueueElementUpgrade(Element&, bool alreadyScheduledToUpgrade);
    static void tryToUpgradeElement(Element&);
    static void enqueueConnectedCallbackIfNeeded(Element&);
    static void enqueueDisconnectedCallbackIfNeeded(Element&);
    static void enqueueAdoptedCallbackIfNeeded(Element&, Document& oldDocument, Document& newDocument);
    static void enqueueAttributeChangedCallbackIfNeeded(Element&, const QualifiedName&, const AtomString& oldValue, const AtomString& newValue);
    static void enqueueFormAssociatedCallbackIfNeeded(Element&, HTMLFormElement*);
    static void enqueueFormDisabledCallbackIfNeeded(Element&, bool isDisabled);
    static void enqueueFormResetCallbackIfNeeded(Element&);
    static void enqueueFormStateRestoreCallbackIfNeeded(Element&, CustomElementFormValue&&);
    static void enqueuePostUpgradeReactions(Element&);

    bool observesStyleAttribute() const;
    bool isElementInternalsDisabled() const;
    bool isElementInternalsAttached() const;
    void setElementInternalsAttached();
    bool isFormAssociated() const;
    bool hasFormStateRestoreCallback() const;

    void invokeAll(Element&);
    void clear();
    bool isEmpty() const { return m_items.isEmpty(); }
#if ASSERT_ENABLED
    bool hasJustUpgradeReaction() const;
#endif

    static void processBackupQueue(CustomElementQueue&);

    static void enqueueElementsOnAppropriateElementQueue(const Vector<Ref<Element>>&);

private:
    static void enqueueElementOnAppropriateElementQueue(Element&);

    using Item = CustomElementReactionQueueItem;

    Ref<JSCustomElementInterface> m_interface;
    Vector<Item> m_items;
    bool m_elementInternalsAttached { false };
};

class CustomElementReactionDisallowedScope {
public:
    CustomElementReactionDisallowedScope()
    {
#if ASSERT_ENABLED
        s_customElementReactionDisallowedCount++;
#endif
    }

    ~CustomElementReactionDisallowedScope()
    {
#if ASSERT_ENABLED
        ASSERT(s_customElementReactionDisallowedCount);
        s_customElementReactionDisallowedCount--;
#endif
    }

#if ASSERT_ENABLED
    static bool isReactionAllowed() { return !s_customElementReactionDisallowedCount; }
#endif

    class AllowedScope {
#if ASSERT_ENABLED
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
#endif // ASSERT_ENABLED
    };

private:
#if ASSERT_ENABLED
    WEBCORE_EXPORT static unsigned s_customElementReactionDisallowedCount;

    friend class AllowedScope;
#endif
};

class CustomElementReactionStack : public CustomElementReactionDisallowedScope::AllowedScope {
public:
    ALWAYS_INLINE CustomElementReactionStack(JSC::JSGlobalObject* state)
        : m_previousProcessingStack(s_currentProcessingStack)
        , m_state(state)
    {
        s_currentProcessingStack = this;
    }

    ALWAYS_INLINE CustomElementReactionStack(JSC::JSGlobalObject& state)
        : CustomElementReactionStack(&state)
    { }

    ALWAYS_INLINE ~CustomElementReactionStack()
    {
        if (UNLIKELY(m_queue))
            processQueue(m_state);
        s_currentProcessingStack = m_previousProcessingStack;
    }

    Vector<GCReachableRef<Element>> takeElements();

private:
    WEBCORE_EXPORT void processQueue(JSC::JSGlobalObject*);

    CustomElementQueue* m_queue { nullptr }; // Use raw pointer to avoid generating delete in the destructor.
    CustomElementReactionStack* const m_previousProcessingStack;
    JSC::JSGlobalObject* const m_state;

    WEBCORE_EXPORT static CustomElementReactionStack* s_currentProcessingStack;

    friend CustomElementReactionQueue;
};

}
