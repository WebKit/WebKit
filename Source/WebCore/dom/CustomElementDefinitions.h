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

#include "JSCustomElementInterface.h"
#include "QualifiedName.h"
#include <wtf/HashMap.h>
#include <wtf/text/AtomicString.h>
#include <wtf/text/AtomicStringHash.h>

#ifndef CustomElementDefinitions_h
#define CustomElementDefinitions_h

#if ENABLE(CUSTOM_ELEMENTS)

namespace JSC {

class JSObject;
    
}

namespace WebCore {

class Element;
class QualifiedName;

class CustomElementDefinitions {
    WTF_MAKE_FAST_ALLOCATED;
public:
    void addElementDefinition(Ref<JSCustomElementInterface>&&);

    JSCustomElementInterface* findInterface(const QualifiedName&) const;
    JSCustomElementInterface* findInterface(const AtomicString&) const;
    JSCustomElementInterface* findInterface(const JSC::JSObject*) const;
    bool containsConstructor(const JSC::JSObject*) const;

    enum class NameStatus { Valid, ConflictsWithBuiltinNames, NoHyphen, ContainsUpperCase };
    static NameStatus checkName(const AtomicString& tagName);

private:
    HashMap<AtomicString, RefPtr<JSCustomElementInterface>> m_nameMap;
    HashMap<const JSC::JSObject*, JSCustomElementInterface*> m_constructorMap;
};
    
}

#endif

#endif /* CustomElementDefinitions_h */
