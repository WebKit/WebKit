/*
 * Copyright (C) 2012, 2013 Google Inc. All rights reserved.
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
#include "ShadowRoot.h"
#include "ShadowRootInit.h"
#include "SlotAssignmentMode.h"
#include "TemplateContentDocumentFragment.h"
#include "markup.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLTemplateElement);

using namespace HTMLNames;

inline HTMLTemplateElement::HTMLTemplateElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document)
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

DocumentFragment& HTMLTemplateElement::content() const
{
    ASSERT(!m_declarativeShadowRoot);
    if (!m_content)
        m_content = TemplateContentDocumentFragment::create(document().ensureTemplateDocument(), *this);
    return *m_content;
}

void HTMLTemplateElement::setDeclarativeShadowRoot(ShadowRoot& shadowRoot)
{
    m_declarativeShadowRoot = shadowRoot;
}

Ref<Node> HTMLTemplateElement::cloneNodeInternal(Document& targetDocument, CloningOperation type)
{
    RefPtr<Node> clone;
    switch (type) {
    case CloningOperation::OnlySelf:
        return cloneElementWithoutChildren(targetDocument);
    case CloningOperation::SelfWithTemplateContent:
        clone = cloneElementWithoutChildren(targetDocument);
        break;
    case CloningOperation::Everything:
        clone = cloneElementWithChildren(targetDocument);
        break;
    }
    if (m_content)
        content().cloneChildNodes(downcast<HTMLTemplateElement>(clone.get())->content());
    return clone.releaseNonNull();
}

void HTMLTemplateElement::didMoveToNewDocument(Document& oldDocument, Document& newDocument)
{
    HTMLElement::didMoveToNewDocument(oldDocument, newDocument);
    if (!m_content)
        return;
    ASSERT_WITH_SECURITY_IMPLICATION(&document() == &newDocument);
    m_content->setTreeScopeRecursively(newDocument.ensureTemplateDocument());
}

void HTMLTemplateElement::attachAsDeclarativeShadowRootIfNeeded(Element& host)
{
    if (m_declarativeShadowRoot) {
        ASSERT(host.shadowRoot());
        return;
    }

    auto modeString = attributeWithoutSynchronization(HTMLNames::shadowrootAttr);
    std::optional<ShadowRootMode> mode;

    if (equalLettersIgnoringASCIICase(modeString, "closed"_s))
        mode = ShadowRootMode::Closed;
    else if (equalLettersIgnoringASCIICase(modeString, "open"_s))
        mode = ShadowRootMode::Open;

    if (!mode)
        return;

    bool delegatesFocus = hasAttributeWithoutSynchronization(HTMLNames::shadowrootdelegatesfocusAttr);

    auto exceptionOrShadowRoot = host.attachDeclarativeShadow(*mode, delegatesFocus);
    if (exceptionOrShadowRoot.hasException())
        return;

    auto importedContent = document().importNode(content(), /* deep */ true).releaseReturnValue();
    for (RefPtr<Node> node = NodeTraversal::next(importedContent), next; node; node = next) {
        next = NodeTraversal::next(*node);
        if (!is<HTMLTemplateElement>(*node))
            continue;
        if (RefPtr parentElement = node->parentElement())
            downcast<HTMLTemplateElement>(*node).attachAsDeclarativeShadowRootIfNeeded(*parentElement);
    }

    Ref shadowRoot = exceptionOrShadowRoot.releaseReturnValue();
    shadowRoot->appendChild(WTFMove(importedContent));

    remove();
}

} // namespace WebCore
