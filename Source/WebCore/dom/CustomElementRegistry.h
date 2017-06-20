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

#include "JSDOMPromiseDeferred.h"
#include "QualifiedName.h"
#include <wtf/HashMap.h>
#include <wtf/text/AtomicString.h>
#include <wtf/text/AtomicStringHash.h>

namespace JSC {

class JSObject;
class JSValue;
    
}

namespace WebCore {

class CustomElementRegistry;
class DOMWindow;
class Element;
class JSCustomElementInterface;
class QualifiedName;

class CustomElementRegistry : public RefCounted<CustomElementRegistry> {
public:
    static Ref<CustomElementRegistry> create(DOMWindow&);
    ~CustomElementRegistry();

    void addElementDefinition(Ref<JSCustomElementInterface>&&);

    bool& elementDefinitionIsRunning() { return m_elementDefinitionIsRunning; }

    JSCustomElementInterface* findInterface(const Element&) const;
    JSCustomElementInterface* findInterface(const QualifiedName&) const;
    JSCustomElementInterface* findInterface(const AtomicString&) const;
    JSCustomElementInterface* findInterface(const JSC::JSObject*) const;
    bool containsConstructor(const JSC::JSObject*) const;

    JSC::JSValue get(const AtomicString&);

    HashMap<AtomicString, Ref<DeferredPromise>>& promiseMap() { return m_promiseMap; }

private:
    CustomElementRegistry(DOMWindow&);

    DOMWindow& m_window;
    HashMap<AtomicString, Ref<JSCustomElementInterface>> m_nameMap;
    HashMap<const JSC::JSObject*, JSCustomElementInterface*> m_constructorMap;
    HashMap<AtomicString, Ref<DeferredPromise>> m_promiseMap;

    bool m_elementDefinitionIsRunning { false };

    friend class ElementDefinitionIsRunningSetForScope;
};

}
