/*
 * Copyright (C) 2015-2018 Apple Inc. All rights reserved.
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
#include "HTMLAttachmentElement.h"

#if ENABLE(ATTACHMENT_ELEMENT)

#include "AttachmentElementClient.h"
#include "DOMURL.h"
#include "Document.h"
#include "Editor.h"
#include "ElementInlines.h"
#include "File.h"
#include "Frame.h"
#include "HTMLImageElement.h"
#include "HTMLNames.h"
#include "MIMETypeRegistry.h"
#include "RenderAttachment.h"
#include "ShadowRoot.h"
#include "SharedBuffer.h"
#include <pal/FileSizeFormatter.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/UUID.h>
#include <wtf/URLParser.h>

#if ENABLE(SERVICE_CONTROLS)
#include "ImageControlsMac.h"
#endif

#if PLATFORM(COCOA)
#include "UTIUtilities.h"
#endif

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLAttachmentElement);

using namespace HTMLNames;

HTMLAttachmentElement::HTMLAttachmentElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document)
{
    ASSERT(hasTagName(attachmentTag));
}

HTMLAttachmentElement::~HTMLAttachmentElement() = default;

Ref<HTMLAttachmentElement> HTMLAttachmentElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new HTMLAttachmentElement(tagName, document));
}

RenderPtr<RenderElement> HTMLAttachmentElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    return createRenderer<RenderAttachment>(*this, WTFMove(style));
}

const String& HTMLAttachmentElement::getAttachmentIdentifier(HTMLImageElement& image)
{
    if (auto attachment = image.attachmentElement())
        return attachment->uniqueIdentifier();

    auto& document = image.document();
    auto attachment = create(HTMLNames::attachmentTag, document);
    auto& identifier = attachment->ensureUniqueIdentifier();

    document.registerAttachmentIdentifier(identifier, image);
    image.setAttachmentElement(WTFMove(attachment));

    return identifier;
}

void HTMLAttachmentElement::copyNonAttributePropertiesFromElement(const Element& source)
{
    m_uniqueIdentifier = downcast<HTMLAttachmentElement>(source).uniqueIdentifier();
    HTMLElement::copyNonAttributePropertiesFromElement(source);
}

URL HTMLAttachmentElement::archiveResourceURL(const String& identifier)
{
    auto resourceURL = URL({ }, "applewebdata://attachment/"_s);
    resourceURL.setPath(identifier);
    return resourceURL;
}

File* HTMLAttachmentElement::file() const
{
    return m_file.get();
}

URL HTMLAttachmentElement::blobURL() const
{
    return { { }, attributeWithoutSynchronization(HTMLNames::webkitattachmentbloburlAttr).string() };
}

void HTMLAttachmentElement::setFile(RefPtr<File>&& file, UpdateDisplayAttributes updateAttributes)
{
    m_file = WTFMove(file);

    if (updateAttributes == UpdateDisplayAttributes::Yes) {
        if (m_file) {
            setAttributeWithoutSynchronization(HTMLNames::titleAttr, AtomString { m_file->name() });
            setAttributeWithoutSynchronization(HTMLNames::subtitleAttr, PAL::fileSizeDescription(m_file->size()));
            setAttributeWithoutSynchronization(HTMLNames::typeAttr, AtomString { m_file->type() });
        } else {
            removeAttribute(HTMLNames::titleAttr);
            removeAttribute(HTMLNames::subtitleAttr);
            removeAttribute(HTMLNames::typeAttr);
        }
    }

    if (auto* renderer = this->renderer())
        renderer->invalidate();
}

Node::InsertedIntoAncestorResult HTMLAttachmentElement::insertedIntoAncestor(InsertionType type, ContainerNode& ancestor)
{
    auto result = HTMLElement::insertedIntoAncestor(type, ancestor);
    if (type.connectedToDocument)
        document().didInsertAttachmentElement(*this);
    return result;
}

void HTMLAttachmentElement::removedFromAncestor(RemovalType type, ContainerNode& ancestor)
{
    HTMLElement::removedFromAncestor(type, ancestor);
    if (type.disconnectedFromDocument)
        document().didRemoveAttachmentElement(*this);
}

const String& HTMLAttachmentElement::ensureUniqueIdentifier()
{
    if (m_uniqueIdentifier.isEmpty())
        m_uniqueIdentifier = createVersion4UUIDString();
    return m_uniqueIdentifier;
}

void HTMLAttachmentElement::setUniqueIdentifier(const String& uniqueIdentifier)
{
    if (m_uniqueIdentifier == uniqueIdentifier)
        return;

    m_uniqueIdentifier = uniqueIdentifier;

    if (auto image = enclosingImageElement())
        image->didUpdateAttachmentIdentifier();
}

RefPtr<HTMLImageElement> HTMLAttachmentElement::enclosingImageElement() const
{
    if (auto hostElement = shadowHost(); is<HTMLImageElement>(hostElement))
        return downcast<HTMLImageElement>(hostElement);

    return { };
}

void HTMLAttachmentElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name == progressAttr || name == subtitleAttr || name == titleAttr || name == typeAttr) {
        if (auto* renderer = this->renderer())
            renderer->invalidate();
    }

    HTMLElement::parseAttribute(name, value);

#if ENABLE(SERVICE_CONTROLS)
    if (name == typeAttr && attachmentType() == "application/pdf"_s) {
        setImageMenuEnabled(true);
        ImageControlsMac::updateImageControls(*this);
    }
#endif
}

String HTMLAttachmentElement::attachmentTitle() const
{
    auto& title = attributeWithoutSynchronization(titleAttr);
    if (!title.isEmpty())
        return title;
    return m_file ? m_file->name() : String();
}

String HTMLAttachmentElement::attachmentTitleForDisplay() const
{
    auto title = attachmentTitle();
    auto indexOfLastDot = title.reverseFind('.');
    if (indexOfLastDot == notFound)
        return title;

    return makeString(
        leftToRightMark,
        firstStrongIsolate,
        StringView(title).left(indexOfLastDot),
        popDirectionalIsolate,
        StringView(title).substring(indexOfLastDot)
    );
}

String HTMLAttachmentElement::attachmentType() const
{
    return attributeWithoutSynchronization(typeAttr);
}

String HTMLAttachmentElement::attachmentPath() const
{
    return attributeWithoutSynchronization(webkitattachmentpathAttr);
}

void HTMLAttachmentElement::updateAttributes(std::optional<uint64_t>&& newFileSize, const AtomString& newContentType, const AtomString& newFilename)
{
    if (!newFilename.isNull()) {
        if (auto enclosingImage = enclosingImageElement())
            enclosingImage->setAttributeWithoutSynchronization(HTMLNames::altAttr, newFilename);
        setAttributeWithoutSynchronization(HTMLNames::titleAttr, newFilename);
    } else {
        if (auto enclosingImage = enclosingImageElement())
            enclosingImage->removeAttribute(HTMLNames::altAttr);
        removeAttribute(HTMLNames::titleAttr);
    }

    if (!newContentType.isNull())
        setAttributeWithoutSynchronization(HTMLNames::typeAttr, newContentType);
    else
        removeAttribute(HTMLNames::typeAttr);

    if (newFileSize)
        setAttributeWithoutSynchronization(HTMLNames::subtitleAttr, PAL::fileSizeDescription(*newFileSize));
    else
        removeAttribute(HTMLNames::subtitleAttr);

    if (auto* renderer = this->renderer())
        renderer->invalidate();
}

static bool mimeTypeIsSuitableForInlineImageAttachment(const String& mimeType)
{
    return MIMETypeRegistry::isSupportedImageMIMEType(mimeType) || MIMETypeRegistry::isPDFMIMEType(mimeType);
}

void HTMLAttachmentElement::updateEnclosingImageWithData(const String& contentType, Ref<FragmentedSharedBuffer>&& buffer)
{
    if (buffer->isEmpty())
        return;

    auto enclosingImage = enclosingImageElement();
    if (!enclosingImage)
        return;

    String mimeType = contentType;
#if PLATFORM(COCOA)
    if (isDeclaredUTI(contentType))
        mimeType = MIMETypeFromUTI(contentType);
#endif

    if (!mimeTypeIsSuitableForInlineImageAttachment(mimeType))
        return;

    enclosingImage->setAttributeWithoutSynchronization(HTMLNames::srcAttr, AtomString { DOMURL::createObjectURL(document(), Blob::create(&document(), buffer->extractData(), mimeType)) });
}

void HTMLAttachmentElement::updateThumbnail(const RefPtr<Image>& thumbnail)
{
    m_thumbnail = thumbnail;
    removeAttribute(HTMLNames::progressAttr);
    if (auto* renderer = this->renderer())
        renderer->invalidate();
}

void HTMLAttachmentElement::updateIcon(const RefPtr<Image>& icon, const WebCore::FloatSize& iconSize)
{
    m_icon = icon;
    m_iconSize = iconSize;

    if (auto* renderer = this->renderer())
        renderer->invalidate();
}

void HTMLAttachmentElement::requestIconWithSize(const FloatSize& size) const
{
    if (!document().page() || !document().page()->attachmentElementClient())
        return;

    document().page()->attachmentElementClient()->requestAttachmentIcon(uniqueIdentifier(), size);
}

#if ENABLE(SERVICE_CONTROLS)
bool HTMLAttachmentElement::childShouldCreateRenderer(const Node& child) const
{
    return hasShadowRootParent(child) && HTMLElement::childShouldCreateRenderer(child);
}
#endif

} // namespace WebCore

#endif // ENABLE(ATTACHMENT_ELEMENT)
