/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "AttachmentAssociatedElement.h"

#if ENABLE(ATTACHMENT_ELEMENT)

#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "ElementChildIteratorInlines.h"
#include "HTMLAttachmentElement.h"
#include "ShadowRoot.h"

namespace WebCore {

void AttachmentAssociatedElement::setAttachmentElement(Ref<HTMLAttachmentElement>&& attachment)
{
    if (auto existingAttachment = attachmentElement())
        existingAttachment->remove();

    attachment->setInlineStyleProperty(CSSPropertyDisplay, CSSValueNone, true);
    asHTMLElement().ensureUserAgentShadowRoot().appendChild(WTFMove(attachment));
}

RefPtr<HTMLAttachmentElement> AttachmentAssociatedElement::attachmentElement() const
{
    if (auto shadowRoot = asHTMLElement().userAgentShadowRoot())
        return childrenOfType<HTMLAttachmentElement>(*shadowRoot).first();

    return nullptr;
}

const String& AttachmentAssociatedElement::attachmentIdentifier() const
{
    if (!m_pendingClonedAttachmentID.isEmpty())
        return m_pendingClonedAttachmentID;

    if (auto attachment = attachmentElement())
        return attachment->uniqueIdentifier();

    return nullAtom();
}

void AttachmentAssociatedElement::didUpdateAttachmentIdentifier()
{
    m_pendingClonedAttachmentID = { };
}

void AttachmentAssociatedElement::copyAttachmentAssociatedPropertiesFromElement(const AttachmentAssociatedElement& source)
{
    m_pendingClonedAttachmentID = !source.m_pendingClonedAttachmentID.isEmpty() ? source.m_pendingClonedAttachmentID : source.attachmentIdentifier();
}

void AttachmentAssociatedElement::cloneAttachmentAssociatedElementWithoutAttributesAndChildren(AttachmentAssociatedElement& clone, Document& targetDocument)
{
    if (auto attachment = attachmentElement()) {
        auto attachmentClone = attachment->cloneElementWithoutChildren(targetDocument);
        clone.setAttachmentElement(checkedDowncast<HTMLAttachmentElement>(attachmentClone.get()));
    }
}

} // namespace WebCore

#endif // ENABLE(ATTACHMENT_ELEMENT)
