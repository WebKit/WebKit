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

#include "CustomElementReactionQueue.h"
#include "Document.h"
#include "ElementRareData.h"
#include "ElementTraversal.h"
#include "JSCustomElementInterface.h"
#include "JSDOMPromiseDeferred.h"
#include "LocalDOMWindow.h"
#include "MathMLNames.h"
#include "QualifiedName.h"
#include "Quirks.h"
#include "ShadowRoot.h"
#include "TypedElementDescendantIteratorInlines.h"
#include <JavaScriptCore/JSCJSValueInlines.h>
#include <wtf/text/AtomString.h>

namespace WebCore {

Ref<CustomElementRegistry> CustomElementRegistry::create(LocalDOMWindow& window, ScriptExecutionContext* scriptExecutionContext)
{
    return adoptRef(*new CustomElementRegistry(window, scriptExecutionContext));
}

CustomElementRegistry::CustomElementRegistry(LocalDOMWindow& window, ScriptExecutionContext* scriptExecutionContext)
    : ContextDestructionObserver(scriptExecutionContext)
    , m_window(window)
{
}

CustomElementRegistry::~CustomElementRegistry() = default;

Document* CustomElementRegistry::document() const
{
    return m_window.document();
}

bool CustomElementRegistry::enqueueKnownNumberOfUpgradesInShadowIncludingTreeOrder(ContainerNode& node, JSCustomElementInterface& elementInterface, unsigned& remainingUpgrades)
{
    for (RefPtr element = ElementTraversal::firstWithin(node); element; element = ElementTraversal::next(*element)) {
        if (element->isCustomElementUpgradeCandidate()) {
            if (element->tagQName().matches(elementInterface.name())) {
                element->enqueueToUpgrade(elementInterface);
                ASSERT(remainingUpgrades > 0);
                --remainingUpgrades;
                if (!remainingUpgrades)
                    return true;
            }
        }
        if (RefPtr shadowRoot = element->shadowRoot(); shadowRoot && shadowRoot->mode() != ShadowRootMode::UserAgent) {
            if (enqueueKnownNumberOfUpgradesInShadowIncludingTreeOrder(*shadowRoot, elementInterface, remainingUpgrades))
                return true;
        }
    }
    return false;
}

void CustomElementRegistry::enqueueUpgradesInShadowIncludingTreeOrderAndUpdateElementCounts(ContainerNode& node, JSCustomElementInterface& elementInterface)
{
    for (RefPtr element = ElementTraversal::firstWithin(node); element; element = ElementTraversal::next(*element)) {
        if (element->isCustomElementUpgradeCandidate()) {
            if (element->tagQName().matches(elementInterface.name()))
                element->enqueueToUpgrade(elementInterface);
            else if (auto* entry = entryForElement(*element)) // returns nullptr for non-HTML elements.
                entry->elementCount++;
        }
        if (RefPtr shadowRoot = element->shadowRoot(); shadowRoot && shadowRoot->mode() != ShadowRootMode::UserAgent)
            enqueueUpgradesInShadowIncludingTreeOrderAndUpdateElementCounts(*shadowRoot, elementInterface);
    }
}

RefPtr<DeferredPromise> CustomElementRegistry::addElementDefinition(Ref<JSCustomElementInterface>&& elementInterface)
{
    static MainThreadNeverDestroyed<const AtomString> extendsLi("extends-li"_s);

    AtomString localName = elementInterface->name().localName();
    auto& entry = m_nameMap.add(localName, Entry { }).iterator->value;
    entry.elementInterface = WTFMove(elementInterface);
    {
        Locker locker { m_constructorMapLock };
        m_constructorMap.add(entry.elementInterface->constructor(), entry.elementInterface.get());
    }

    if (entry.elementInterface->isShadowDisabled())
        m_disabledShadowSet.add(localName);

    if (RefPtr document = m_window.document()) {
        // ungap/@custom-elements detection for quirk (rdar://problem/111008826).
        if (localName == extendsLi.get())
            document->quirks().setNeedsConfigurableIndexedPropertiesQuirk();
        if (LIKELY(m_elementCountsAreSet)) {
            if (auto elementCount = entry.elementCount) {
                bool exitedEarly = enqueueKnownNumberOfUpgradesInShadowIncludingTreeOrder(*document, *entry.elementInterface, elementCount);
                ASSERT_UNUSED(exitedEarly, exitedEarly);
            }
        } else {
            // This is the very first customElements.upgarde call. Traverse the entire document and update element counts.
            enqueueUpgradesInShadowIncludingTreeOrderAndUpdateElementCounts(*document, *entry.elementInterface);
            m_elementCountsAreSet = true;
        }
    }

    return m_promiseMap.take(localName);
}

auto CustomElementRegistry::entryForElement(const Element& element) -> Entry*
{
    auto& qualifiedName = element.tagQName();
    if (qualifiedName.namespaceURI() != HTMLNames::xhtmlNamespaceURI)
        return nullptr;
    return &m_nameMap.ensure(qualifiedName.localName(), []() { return Entry { }; }).iterator->value;
}

JSCustomElementInterface* CustomElementRegistry::findInterface(const Element& element) const
{
    return findInterface(element.tagQName());
}

JSCustomElementInterface* CustomElementRegistry::findInterface(const QualifiedName& name) const
{
    if (name.namespaceURI() != HTMLNames::xhtmlNamespaceURI)
        return nullptr;
    return findInterface(name.localName());
}

JSCustomElementInterface* CustomElementRegistry::findInterface(const AtomString& name) const
{
    auto it = m_nameMap.find(name);
    if (it == m_nameMap.end())
        return nullptr;
    return it->value.elementInterface.get();
}

JSCustomElementInterface* CustomElementRegistry::findInterface(const JSC::JSObject* constructor) const
{
    Locker locker { m_constructorMapLock };
    return m_constructorMap.get(constructor);
}

bool CustomElementRegistry::containsConstructor(const JSC::JSObject* constructor) const
{
    Locker locker { m_constructorMapLock };
    return m_constructorMap.contains(constructor);
}

JSC::JSValue CustomElementRegistry::get(const AtomString& name)
{
    if (auto* elementInterface = findInterface(name))
        return elementInterface->constructor();
    return JSC::jsUndefined();
}

String CustomElementRegistry::getName(JSC::JSValue constructorValue)
{
    auto* constructor = constructorValue.getObject();
    if (!constructor)
        return String { };
    auto* elementInterface = findInterface(constructor);
    if (!elementInterface)
        return String { };
    return elementInterface->name().localName();
}

static void upgradeElementsInShadowIncludingDescendants(ContainerNode& root)
{
    for (auto& element : descendantsOfType<Element>(root)) {
        if (element.isCustomElementUpgradeCandidate())
            CustomElementReactionQueue::tryToUpgradeElement(element);
        if (auto* shadowRoot = element.shadowRoot())
            upgradeElementsInShadowIncludingDescendants(*shadowRoot);
    }
}

void CustomElementRegistry::upgrade(Node& root)
{
    if (!is<ContainerNode>(root))
        return;

    if (is<Element>(root) && downcast<Element>(root).isCustomElementUpgradeCandidate())
        CustomElementReactionQueue::tryToUpgradeElement(downcast<Element>(root));

    upgradeElementsInShadowIncludingDescendants(downcast<ContainerNode>(root));
}

template<typename Visitor>
void CustomElementRegistry::visitJSCustomElementInterfaces(Visitor& visitor) const
{
    Locker locker { m_constructorMapLock };
    for (const auto& iterator : m_constructorMap)
        iterator.value->visitJSFunctions(visitor);
}

template void CustomElementRegistry::visitJSCustomElementInterfaces(JSC::AbstractSlotVisitor&) const;
template void CustomElementRegistry::visitJSCustomElementInterfaces(JSC::SlotVisitor&) const;

}
