/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 *           (C) 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
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

#include "EventListenerMap.h"
#include "EventTargetInterfaces.h"
#include "ExceptionOr.h"
#include "ScriptWrappable.h"
#include <memory>
#include <wtf/Forward.h>
#include <wtf/IsoMalloc.h>
#include <wtf/Variant.h>

namespace WebCore {

class DOMWrapperWorld;

struct EventTargetData {
    WTF_MAKE_NONCOPYABLE(EventTargetData); WTF_MAKE_FAST_ALLOCATED;
public:
    EventTargetData() = default;
    EventListenerMap eventListenerMap;
    bool isFiringEventListeners { false };
};

class EventTarget : public ScriptWrappable {
    WTF_MAKE_ISO_ALLOCATED(EventTarget);
public:
    void ref() { refEventTarget(); }
    void deref() { derefEventTarget(); }

    virtual EventTargetInterface eventTargetInterface() const = 0;
    virtual ScriptExecutionContext* scriptExecutionContext() const = 0;

    virtual bool isNode() const;
    virtual bool isPaymentRequest() const;

    struct ListenerOptions {
        ListenerOptions(bool capture = false)
            : capture(capture)
        { }

        bool capture { false };
    };

    struct AddEventListenerOptions : ListenerOptions {
        AddEventListenerOptions(bool capture = false, Optional<bool> passive = WTF::nullopt, bool once = false)
            : ListenerOptions(capture)
            , passive(passive)
            , once(once)
        { }

        Optional<bool> passive;
        bool once { false };
    };

    using AddEventListenerOptionsOrBoolean = Variant<AddEventListenerOptions, bool>;
    WEBCORE_EXPORT void addEventListenerForBindings(const AtomString& eventType, RefPtr<EventListener>&&, AddEventListenerOptionsOrBoolean&&);
    using ListenerOptionsOrBoolean = Variant<ListenerOptions, bool>;
    WEBCORE_EXPORT void removeEventListenerForBindings(const AtomString& eventType, RefPtr<EventListener>&&, ListenerOptionsOrBoolean&&);
    WEBCORE_EXPORT ExceptionOr<bool> dispatchEventForBindings(Event&);

    WEBCORE_EXPORT virtual bool addEventListener(const AtomString& eventType, Ref<EventListener>&&, const AddEventListenerOptions& = { });
    virtual bool removeEventListener(const AtomString& eventType, EventListener&, const ListenerOptions&);

    virtual void removeAllEventListeners();
    virtual void dispatchEvent(Event&);
    virtual void uncaughtExceptionInEventHandler();

    // Used for legacy "onevent" attributes.
    bool setAttributeEventListener(const AtomString& eventType, RefPtr<EventListener>&&, DOMWrapperWorld&);
    EventListener* attributeEventListener(const AtomString& eventType, DOMWrapperWorld&);

    bool hasEventListeners() const;
    bool hasEventListeners(const AtomString& eventType) const;
    bool hasCapturingEventListeners(const AtomString& eventType);
    bool hasActiveEventListeners(const AtomString& eventType) const;

    Vector<AtomString> eventTypes();
    const EventListenerVector& eventListeners(const AtomString& eventType);

    enum class EventInvokePhase { Capturing, Bubbling };
    void fireEventListeners(Event&, EventInvokePhase);
    bool isFiringEventListeners() const;

    void visitJSEventListeners(JSC::SlotVisitor&);
    void invalidateJSEventListeners(JSC::JSObject*);

protected:
    virtual ~EventTarget() = default;
    
    virtual EventTargetData* eventTargetData() = 0;
    virtual EventTargetData* eventTargetDataConcurrently() = 0;
    virtual EventTargetData& ensureEventTargetData() = 0;
    const EventTargetData* eventTargetData() const;

private:
    virtual void refEventTarget() = 0;
    virtual void derefEventTarget() = 0;
    
    void innerInvokeEventListeners(Event&, EventListenerVector, EventInvokePhase);

    friend class EventListenerIterator;
};

class EventTargetWithInlineData : public EventTarget {
    WTF_MAKE_ISO_ALLOCATED(EventTargetWithInlineData);
protected:
    EventTargetData* eventTargetData() final { return &m_eventTargetData; }
    EventTargetData* eventTargetDataConcurrently() final { return &m_eventTargetData; }
    EventTargetData& ensureEventTargetData() final { return m_eventTargetData; }
private:
    EventTargetData m_eventTargetData;
};

inline const EventTargetData* EventTarget::eventTargetData() const
{
    return const_cast<EventTarget*>(this)->eventTargetData();
}

inline bool EventTarget::isFiringEventListeners() const
{
    auto* data = eventTargetData();
    return data && data->isFiringEventListeners;
}

inline bool EventTarget::hasEventListeners() const
{
    auto* data = eventTargetData();
    return data && !data->eventListenerMap.isEmpty();
}

inline bool EventTarget::hasEventListeners(const AtomString& eventType) const
{
    auto* data = eventTargetData();
    return data && data->eventListenerMap.contains(eventType);
}

inline bool EventTarget::hasCapturingEventListeners(const AtomString& eventType)
{
    auto* data = eventTargetData();
    return data && data->eventListenerMap.containsCapturing(eventType);
}

} // namespace WebCore
