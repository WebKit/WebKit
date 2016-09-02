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

#include "config.h"
#include "CustomElementRegistry.h"

#if ENABLE(CUSTOM_ELEMENTS)

#include "CustomElementReactionQueue.h"
#include "DOMWindow.h"
#include "Document.h"
#include "Element.h"
#include "ElementTraversal.h"
#include "JSCustomElementInterface.h"
#include "JSDOMPromise.h"
#include "MathMLNames.h"
#include "QualifiedName.h"
#include "SVGNames.h"
#include "ShadowRoot.h"
#include <runtime/JSCJSValueInlines.h>
#include <wtf/text/AtomicString.h>

namespace WebCore {

Ref<CustomElementRegistry> CustomElementRegistry::create(DOMWindow& window)
{
    return adoptRef(*new CustomElementRegistry(window));
}

CustomElementRegistry::CustomElementRegistry(DOMWindow& window)
    : m_window(window)
{ }

CustomElementRegistry::~CustomElementRegistry()
{ }

// https://dom.spec.whatwg.org/#concept-shadow-including-tree-order
static void enqueueUpgradeInShadowIncludingTreeOrder(ContainerNode& node, JSCustomElementInterface& elementInterface)
{
    for (Element* element = ElementTraversal::firstWithin(node); element; element = ElementTraversal::next(*element)) {
        if (element->isUnresolvedCustomElement() && element->tagQName() == elementInterface.name())
            CustomElementReactionQueue::enqueueElementUpgrade(*element, elementInterface);
        if (auto* shadowRoot = element->shadowRoot()) {
            if (shadowRoot->mode() != ShadowRoot::Mode::UserAgent)
                enqueueUpgradeInShadowIncludingTreeOrder(*shadowRoot, elementInterface);
        }
    }
}

void CustomElementRegistry::addElementDefinition(Ref<JSCustomElementInterface>&& elementInterface)
{
    AtomicString localName = elementInterface->name().localName();
    ASSERT(!m_nameMap.contains(localName));
    m_constructorMap.add(elementInterface->constructor(), elementInterface.ptr());
    m_nameMap.add(localName, elementInterface.copyRef());

    if (auto* document = m_window.document())
        enqueueUpgradeInShadowIncludingTreeOrder(*document, elementInterface.get());
}

JSCustomElementInterface* CustomElementRegistry::findInterface(const Element& element) const
{
    return findInterface(element.tagQName());
}

JSCustomElementInterface* CustomElementRegistry::findInterface(const QualifiedName& name) const
{
    ASSERT(!name.hasPrefix());
    if (name.namespaceURI() != HTMLNames::xhtmlNamespaceURI)
        return nullptr;
    auto it = m_nameMap.find(name.localName());
    return it == m_nameMap.end() || it->value->name() != name ? nullptr : const_cast<JSCustomElementInterface*>(it->value.ptr());
}

JSCustomElementInterface* CustomElementRegistry::findInterface(const AtomicString& name) const
{
    return m_nameMap.get(name);
}

JSCustomElementInterface* CustomElementRegistry::findInterface(const JSC::JSObject* constructor) const
{
    return m_constructorMap.get(constructor);
}

bool CustomElementRegistry::containsConstructor(const JSC::JSObject* constructor) const
{
    return m_constructorMap.contains(constructor);
}

JSC::JSValue CustomElementRegistry::get(const AtomicString& name)
{
    if (auto* elementInterface = m_nameMap.get(name))
        return elementInterface->constructor();
    return JSC::jsUndefined();
}

}

#endif
