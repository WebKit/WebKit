/*
 * Copyright (C) 2007-2017 Apple Inc. All rights reserved.
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

#include "ActiveDOMObject.h"
#include "AttachmentAssociatedElement.h"
#include "HTMLElement.h"
#include "MediaQuery.h"
#include "Timer.h"

namespace WebCore {

class HTMLSourceElement final
    : public HTMLElement
#if ENABLE(ATTACHMENT_ELEMENT)
    , public AttachmentAssociatedElement
#endif
    , public ActiveDOMObject {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(HTMLSourceElement);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(HTMLSourceElement);
public:
    static Ref<HTMLSourceElement> create(Document&);
    static Ref<HTMLSourceElement> create(const QualifiedName&, Document&);

    // ActiveDOMObject.
    void ref() const final { HTMLElement::ref(); }
    void deref() const final { HTMLElement::deref(); }

    void scheduleErrorEvent();
    void cancelPendingErrorEvent();

    const MQ::MediaQueryList& parsedMediaAttribute(Document&) const;

private:
    HTMLSourceElement(const QualifiedName&, Document&);
    
    InsertedIntoAncestorResult insertedIntoAncestor(InsertionType, ContainerNode&) final;
    void removedFromAncestor(RemovalType, ContainerNode&) final;
    void didMoveToNewDocument(Document& oldDocument, Document& newDocument) final;

    bool isURLAttribute(const Attribute&) const final;
    bool attributeContainsURL(const Attribute&) const final;
    Attribute replaceURLsInAttributeValue(const Attribute&, const UncheckedKeyHashMap<String, String>&) const override;
    void addCandidateSubresourceURLs(ListHashSet<URL>&) const override;

    // ActiveDOMObject.
    void stop() final;

#if ENABLE(ATTACHMENT_ELEMENT)
    HTMLSourceElement& asHTMLElement() final { return *this; }
    const HTMLSourceElement& asHTMLElement() const final { return *this; }

    void refAttachmentAssociatedElement() const final { HTMLElement::ref(); }
    void derefAttachmentAssociatedElement() const final { HTMLElement::deref(); }

    AttachmentAssociatedElement* asAttachmentAssociatedElement() final { return this; }

    AttachmentAssociatedElementType attachmentAssociatedElementType() const final { return AttachmentAssociatedElementType::Source; }
#endif

    Ref<Element> cloneElementWithoutAttributesAndChildren(TreeScope&) final;
    void copyNonAttributePropertiesFromElement(const Element&) final;

    void attributeChanged(const QualifiedName&, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason) final;

    TaskCancellationGroup m_errorEventCancellationGroup;
    bool m_shouldCallSourcesChanged { false };
    mutable std::optional<MQ::MediaQueryList> m_cachedParsedMediaAttribute;
};

} // namespace WebCore
