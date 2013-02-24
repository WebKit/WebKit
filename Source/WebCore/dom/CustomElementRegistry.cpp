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

#include "config.h"

#if ENABLE(CUSTOM_ELEMENTS)

#include "CustomElementRegistry.h"

#include "CustomElementConstructor.h"
#include "CustomElementHelpers.h"
#include "Dictionary.h"
#include "Document.h"
#include "HTMLElement.h"
#include "HTMLNames.h"
#include "RuntimeEnabledFeatures.h"
#include <wtf/ASCIICType.h>

#if ENABLE(SVG)
#include "SVGNames.h"
#endif

#if ENABLE(MATHML)
#include "MathMLNames.h"
#endif

namespace WebCore {

CustomElementRegistry::CustomElementRegistry(Document* document)
    : ContextDestructionObserver(document)
{
}

CustomElementRegistry::~CustomElementRegistry()
{
}

PassRefPtr<CustomElementConstructor> CustomElementRegistry::constructorOf(HTMLElement* element)
{
    RefPtr<CustomElementRegistry> self = element->document()->registry();
    if (!self)
        return 0;
    return self->find(element->tagQName());
}

bool CustomElementRegistry::isValidName(const AtomicString& name)
{
    size_t hyphenPosition = name.find('-');
    if (hyphenPosition == notFound)
        return false;

    DEFINE_STATIC_LOCAL(Vector<AtomicString>, reservedNames, ());
    if (reservedNames.isEmpty()) {
#if ENABLE(MATHML)
        reservedNames.append(MathMLNames::annotation_xmlTag.localName());
#endif

#if ENABLE(SVG)
        reservedNames.append(SVGNames::color_profileTag.localName());
        reservedNames.append(SVGNames::font_faceTag.localName());
        reservedNames.append(SVGNames::font_face_srcTag.localName());
        reservedNames.append(SVGNames::font_face_uriTag.localName());
        reservedNames.append(SVGNames::font_face_formatTag.localName());
        reservedNames.append(SVGNames::font_face_nameTag.localName());
        reservedNames.append(SVGNames::missing_glyphTag.localName());
#endif
    }

    if (notFound != reservedNames.find(name))
        return false;

    return Document::isValidName(name.string());
}

PassRefPtr<CustomElementConstructor> CustomElementRegistry::registerElement(ScriptState* state, const AtomicString& name, const Dictionary& options, ExceptionCode& ec)
{
    RefPtr<CustomElementRegistry> protect(this);

    if (!CustomElementHelpers::isFeatureAllowed(state))
        return 0;

    QualifiedName newName(nullAtom, name.lower(), HTMLNames::xhtmlNamespaceURI);
    if (!isValidName(newName.localName())) {
        ec = INVALID_CHARACTER_ERR;
        return 0;
    }
        
    if (find(newName)) {
        ec = INVALID_STATE_ERR;
        return 0;
    }

    ScriptValue prototypeValue;
    if (!options.get("prototype", prototypeValue)) {
        // FIXME: Implement the default value handling.
        // Currently default value of the "prototype" parameter, which
        // is HTMLSpanElement.prototype, has an ambiguity about its
        // behavior. The spec should be fixed before WebKit implements
        // it. https://www.w3.org/Bugs/Public/show_bug.cgi?id=20801
        ec = INVALID_STATE_ERR;
        return 0;
    }

    if (!CustomElementHelpers::isValidPrototypeParameter(prototypeValue, state)) {
        ec = INVALID_STATE_ERR;
        return 0;
    }
    
    // An script execution could happen in isValidPrototypeParameter(), which kills the document.
    if (!document()) {
        ec = INVALID_STATE_ERR;
        return 0;
    }

    RefPtr<CustomElementConstructor> constructor = CustomElementConstructor::create(state, document(), newName, "HTMLCustomElement", prototypeValue);
    if (!constructor) {
        ec = INVALID_STATE_ERR;
        return 0;
    }
        
    m_constructors.add(constructor->tagName().impl(), constructor);
    return constructor;
}

PassRefPtr<CustomElementConstructor> CustomElementRegistry::find(const QualifiedName& name) const
{
    ConstructorMap::const_iterator found = m_constructors.find(name.impl());
    return (found != m_constructors.end()) ? found->value : 0;
}

PassRefPtr<HTMLElement> CustomElementRegistry::createElement(const QualifiedName& name) const
{
    if (RefPtr<CustomElementConstructor> found = find(name))
        return found->createElement();
    return 0;
}

inline Document* CustomElementRegistry::document() const
{
    return static_cast<Document*>(m_scriptExecutionContext);
}

}

#endif // ENABLE(CUSTOM_ELEMENTS)
