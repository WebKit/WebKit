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

#include "ContextDestructionObserver.h"
#include "EventTarget.h"
#include "QualifiedName.h"
#include <wtf/Lock.h>
#include <wtf/RobinHoodHashMap.h>
#include <wtf/RobinHoodHashSet.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/AtomString.h>
#include <wtf/text/AtomStringHash.h>

namespace JSC {

class JSGlobalObject;
class JSObject;
class JSValue;

}

namespace WebCore {

class CustomElementRegistry;
class LocalDOMWindow;
class DeferredPromise;
class Document;
class Element;
class JSCustomElementInterface;
class Node;
class QualifiedName;

class CustomElementRegistry : public RefCounted<CustomElementRegistry>, public ContextDestructionObserver {
public:
    static Ref<CustomElementRegistry> create(LocalDOMWindow&, ScriptExecutionContext*);
    ~CustomElementRegistry();

    Document* document() const;

    RefPtr<DeferredPromise> addElementDefinition(Ref<JSCustomElementInterface>&&);

    bool& elementDefinitionIsRunning() { return m_elementDefinitionIsRunning; }

    JSCustomElementInterface* findInterface(const Element&) const;
    JSCustomElementInterface* findInterface(const QualifiedName&) const;
    JSCustomElementInterface* findInterface(const AtomString&) const;
    RefPtr<JSCustomElementInterface> findInterface(const JSC::JSObject*) const;
    bool containsConstructor(const JSC::JSObject*) const;

    JSC::JSValue get(const AtomString&);
    String getName(JSC::JSValue);
    void upgrade(Node& root);

    MemoryCompactRobinHoodHashMap<AtomString, Ref<DeferredPromise>>& promiseMap() { return m_promiseMap; }
    bool isShadowDisabled(const AtomString& name) const { return m_disabledShadowSet.contains(name); }

    template<typename Visitor> void visitJSCustomElementInterfaces(Visitor&) const;
private:
    CustomElementRegistry(LocalDOMWindow&, ScriptExecutionContext*);

    WeakPtr<LocalDOMWindow, WeakPtrImplWithEventTargetData> m_window;
    HashMap<AtomString, Ref<JSCustomElementInterface>> m_nameMap;
    HashMap<const JSC::JSObject*, JSCustomElementInterface*> m_constructorMap WTF_GUARDED_BY_LOCK(m_constructorMapLock);
    MemoryCompactRobinHoodHashMap<AtomString, Ref<DeferredPromise>> m_promiseMap;
    MemoryCompactRobinHoodHashSet<AtomString> m_disabledShadowSet;

    bool m_elementDefinitionIsRunning { false };
    mutable Lock m_constructorMapLock;

    friend class ElementDefinitionIsRunningSetForScope;
};

}
