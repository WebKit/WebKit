/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#include "config.h"
#include "CustomElementDefinitions.h"

#if ENABLE(CUSTOM_ELEMENTS)

#include "Document.h"
#include "Element.h"
#include "JSCustomElementInterface.h"
#include "MathMLNames.h"
#include "QualifiedName.h"
#include "SVGNames.h"
#include <runtime/JSCJSValueInlines.h>
#include <wtf/text/AtomicString.h>

namespace WebCore {

CustomElementDefinitions::NameStatus CustomElementDefinitions::checkName(const AtomicString& tagName)
{
    bool containsHyphen = false;
    for (unsigned i = 0; i < tagName.length(); i++) {
        if (isASCIIUpper(tagName[i]))
            return NameStatus::ContainsUpperCase;
        if (tagName[i] == '-')
            containsHyphen = true;
    }

    if (!containsHyphen)
        return NameStatus::NoHyphen;

    // FIXME: We should be taking the advantage of QualifiedNames in SVG and MathML.
    if (tagName == SVGNames::color_profileTag.localName()
        || tagName == SVGNames::font_faceTag.localName()
        || tagName == SVGNames::font_face_formatTag.localName()
        || tagName == SVGNames::font_face_nameTag.localName()
        || tagName == SVGNames::font_face_srcTag.localName()
        || tagName == SVGNames::font_face_uriTag.localName()
        || tagName == SVGNames::missing_glyphTag.localName()
#if ENABLE(MATHML)
        || tagName == MathMLNames::annotation_xmlTag.localName()
#endif
        )
        return NameStatus::ConflictsWithBuiltinNames;

    return NameStatus::Valid;
}

void CustomElementDefinitions::addElementDefinition(Ref<JSCustomElementInterface>&& interface)
{
    AtomicString localName = interface->name().localName();
    ASSERT(!m_nameMap.contains(localName));
    m_constructorMap.add(interface->constructor(), interface.ptr());
    m_nameMap.add(localName, WTFMove(interface));
}

JSCustomElementInterface* CustomElementDefinitions::findInterface(const QualifiedName& name) const
{
    auto it = m_nameMap.find(name.localName());
    return it == m_nameMap.end() || it->value->name() != name ? nullptr : it->value.get();
}

JSCustomElementInterface* CustomElementDefinitions::findInterface(const AtomicString& name) const
{
    auto it = m_nameMap.find(name);
    return it == m_nameMap.end() ? nullptr : it->value.get();
}

JSCustomElementInterface* CustomElementDefinitions::findInterface(const JSC::JSObject* constructor) const
{
    auto it = m_constructorMap.find(constructor);
    return it == m_constructorMap.end() ? nullptr : it->value;
}

bool CustomElementDefinitions::containsConstructor(const JSC::JSObject* constructor) const
{
    return m_constructorMap.contains(constructor);
}

}

#endif
