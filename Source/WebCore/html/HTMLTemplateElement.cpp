/*
 * Copyright (C) 2012, 2013 Google Inc. All rights reserved.
 * Copyright (C) 2013-2026 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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
#include "HTMLTemplateElement.h"

#include "Document.h"
#include "DocumentFragment.h"
#include "ElementInlines.h"
#include "ElementRareData.h"
#include "HTMLNames.h"
#include "NodeTraversal.h"
#include "ProcessingInstruction.h"
#include "SerializedNode.h"
#include "ShadowRoot.h"
#include "ShadowRootInit.h"
#include "ShadowRootMode.h"
#include "SlotAssignmentMode.h"
#include "TemplateContentDocumentFragment.h"
#include "markup.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(HTMLTemplateElement);

using namespace HTMLNames;

inline HTMLTemplateElement::HTMLTemplateElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document, TypeFlag::HasDidMoveToNewDocument)
{
}

HTMLTemplateElement::~HTMLTemplateElement()
{
    if (m_content)
        m_content->clearHost();
}

Ref<HTMLTemplateElement> HTMLTemplateElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new HTMLTemplateElement(tagName, document));
}

DocumentFragment* HTMLTemplateElement::contentIfAvailable() const
{
    return m_content.get();
}

DocumentFragment& HTMLTemplateElement::fragmentForInsertion() const
{
    if (m_declarativeShadowRoot)
        return *m_declarativeShadowRoot;
    return content();
}

ContainerNode& HTMLTemplateElement::insertionTarget() const
{
    if (m_insertionTarget)
        return *m_insertionTarget;
    return fragmentForInsertion();
}

Node* HTMLTemplateElement::insertionNextChild() const
{
    if (m_insertionEndMarker && m_insertionEndMarker->parentNode() == m_insertionTarget)
        return m_insertionEndMarker.get();
    return nullptr;
}

bool HTMLTemplateElement::prepareContentPatching(ContainerNode& scope)
{
    auto markerName = attributeWithoutSynchronization(forAttr);
    if (markerName.isEmpty())
        return false;

    for (RefPtr node = scope.firstChild(); node; node = NodeTraversal::next(*node, &scope)) {
        RefPtr instruction = dynamicDowncast<ProcessingInstruction>(*node);
        if (!instruction || instruction->pseudoAttributeValue("name"_s) != markerName)
            continue;

        if (instruction->target() == "marker"_s) {
            m_insertionTarget = instruction->parentNode();
            m_insertionStartMarker = instruction;
            m_insertionEndMarker = instruction;
            return true;
        }

        if (instruction->target() != "start"_s)
            continue;

        RefPtr parent = instruction->parentNode();
        if (!parent)
            return false;

        unsigned nestingLevel = 0;
        Vector<Ref<Node>> nodesToRemove;
        RefPtr<ProcessingInstruction> endMarker;
        for (RefPtr sibling = instruction->nextSibling(); sibling; sibling = sibling->nextSibling()) {
            if (RefPtr siblingInstruction = dynamicDowncast<ProcessingInstruction>(*sibling)) {
                if (siblingInstruction->target() == "start"_s)
                    ++nestingLevel;
                else if (siblingInstruction->target() == "end"_s) {
                    if (!nestingLevel) {
                        endMarker = WTF::move(siblingInstruction);
                        break;
                    }
                    --nestingLevel;
                }
            }
            nodesToRemove.append(*sibling);
        }

        m_insertionTarget = WTF::move(parent);
        m_insertionStartMarker = WTF::move(instruction);
        m_insertionEndMarker = WTF::move(endMarker);

        for (Ref nodeToRemove : nodesToRemove)
            nodeToRemove->remove();
        return true;
    }
    return false;
}

DocumentFragment& HTMLTemplateElement::content() const
{
    ASSERT(!m_declarativeShadowRoot);
    if (!m_content)
        lazyInitialize(m_content, TemplateContentDocumentFragment::create(protect(protect(document())->ensureTemplateDocument()), *this));
    return *m_content;
}

void HTMLTemplateElement::adoptDeserializedContent(Ref<TemplateContentDocumentFragment>&& content)
{
    lazyInitialize(m_content, WTF::move(content));
}

const AtomString& HTMLTemplateElement::shadowRootMode() const
{
    auto modeString = attributeWithoutSynchronization(HTMLNames::shadowrootmodeAttr);
    auto mode = parseShadowRootMode(modeString);
    if (!mode)
        return emptyAtom();
    return serializeShadowRootMode(*mode);
}

const AtomString& HTMLTemplateElement::shadowRootSlotAssignment() const
{
    auto value = attributeWithoutSynchronization(HTMLNames::shadowrootslotassignmentAttr);
    return serializeSlotAssignmentMode(parseSlotAssignmentMode(value));
}

void HTMLTemplateElement::setDeclarativeShadowRoot(ShadowRoot& shadowRoot)
{
    m_declarativeShadowRoot = shadowRoot;
}

void HTMLTemplateElement::finishParsingChildren()
{
    HTMLElement::finishParsingChildren();

    RefPtr insertionTarget = std::exchange(m_insertionTarget, nullptr);
    RefPtr startMarker = std::exchange(m_insertionStartMarker, nullptr);
    RefPtr endMarker = std::exchange(m_insertionEndMarker, nullptr);
    if (!insertionTarget || !startMarker)
        return;

    if (RefPtr parent = startMarker->parentNode())
        parent->parserRemoveChild(*startMarker);
    if (endMarker && endMarker != startMarker) {
        if (RefPtr parent = endMarker->parentNode())
            parent->parserRemoveChild(*endMarker);
    }
}

Ref<Node> HTMLTemplateElement::cloneNodeInternal(Document& document, CloningOperation type, CustomElementRegistry* registry) const
{
    RefPtr<Node> clone;
    switch (type) {
    case CloningOperation::SelfOnly:
        return cloneElementWithoutChildren(document, registry);
    case CloningOperation::SelfWithTemplateContent:
        clone = cloneElementWithoutChildren(document, registry);
        break;
    case CloningOperation::Everything:
        clone = cloneElementWithChildren(document, registry);
        break;
    }
    if (m_content) {
        auto& templateElement = downcast<HTMLTemplateElement>(*clone);
        Ref fragment = templateElement.content();
        m_content->cloneChildNodes(protect(fragment->document()), nullptr, fragment);
    }
    return clone.releaseNonNull();
}

SerializedNode HTMLTemplateElement::serializeNode(CloningOperation type) const
{
    Vector<SerializedNode> children;
    switch (type) {
    case CloningOperation::SelfOnly:
    case CloningOperation::SelfWithTemplateContent:
        break;
    case CloningOperation::Everything:
        children = serializeChildNodes();
        break;
    }

    auto contentChildren = m_content && type != CloningOperation::SelfOnly
        ? std::optional(SerializedNode::DocumentFragment { m_content->serializeChildNodes() })
        : std::nullopt;

    return { SerializedNode::HTMLTemplateElement {
        SerializedNode::Element {
            { WTF::move(children) },
            { tagQName() },
            serializeAttributes<SerializedNode::Element::Attribute>(),
            serializeShadowRoot<SerializedNode::ShadowRoot>()
        }, WTF::move(contentChildren)
    } };
}

void HTMLTemplateElement::didMoveToNewDocument(Document& oldDocument, Document& newDocument)
{
    HTMLElement::didMoveToNewDocument(oldDocument, newDocument);
    if (!m_content)
        return;
    ASSERT_WITH_SECURITY_IMPLICATION(&document() == &newDocument);
    m_content->setTreeScopeRecursively(protect(newDocument.ensureTemplateDocument()));
}

} // namespace WebCore
