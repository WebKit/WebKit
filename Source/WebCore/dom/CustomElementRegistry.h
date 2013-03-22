/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Google Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CustomElementRegistry_h
#define CustomElementRegistry_h

#if ENABLE(CUSTOM_ELEMENTS)

#include "ContextDestructionObserver.h"
#include "ExceptionCode.h"
#include "QualifiedName.h"
#include "ScriptValue.h"
#include "Supplementable.h"
#include <wtf/Forward.h>
#include <wtf/HashSet.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class CustomElementConstructor;
class Dictionary;
class Document;
class Element;
class ScriptExecutionContext;
class QualifiedName;

class CustomElementRegistry : public RefCounted<CustomElementRegistry> , public ContextDestructionObserver {
    WTF_MAKE_NONCOPYABLE(CustomElementRegistry); WTF_MAKE_FAST_ALLOCATED;
public:
    explicit CustomElementRegistry(Document*);
    ~CustomElementRegistry();

    PassRefPtr<CustomElementConstructor> registerElement(WebCore::ScriptState*, const AtomicString& name, const Dictionary& options, ExceptionCode&);
    PassRefPtr<CustomElementConstructor> findFor(Element*) const;
    PassRefPtr<CustomElementConstructor> find(const QualifiedName& elementName, const QualifiedName& localName) const;
    PassRefPtr<Element> createElement(const QualifiedName& localName, const AtomicString& typeExtension) const;

    Document* document() const;

private:
    static bool isValidName(const AtomicString&);

    typedef HashMap<std::pair<QualifiedName, QualifiedName>, RefPtr<CustomElementConstructor> > ConstructorMap;
    typedef HashSet<QualifiedName> NameSet;

    ConstructorMap m_constructors;
    NameSet m_names;
};

} // namespace WebCore

#endif // ENABLE(CUSTOM_ELEMENTS)
#endif
