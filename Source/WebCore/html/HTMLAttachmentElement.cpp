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

#include "DOMURL.h"
#include "Document.h"
#include "Editor.h"
#include "File.h"
#include "FileReaderLoader.h"
#include "FileReaderLoaderClient.h"
#include "Frame.h"
#include "HTMLImageElement.h"
#include "HTMLNames.h"
#include "HTMLVideoElement.h"
#include "MIMETypeRegistry.h"
#include "RenderAttachment.h"
#include "RenderBlockFlow.h"
#include "ShadowRoot.h"
#include "SharedBuffer.h"
#include <pal/FileSizeFormatter.h>

#if PLATFORM(COCOA)
#include "UTIUtilities.h"
#endif

namespace WebCore {

using namespace HTMLNames;

class AttachmentDataReader : public FileReaderLoaderClient {
public:
    static std::unique_ptr<AttachmentDataReader> create(HTMLAttachmentElement& attachment, Function<void(RefPtr<SharedBuffer>&&)>&& callback)
    {
        return std::make_unique<AttachmentDataReader>(attachment, WTFMove(callback));
    }

    AttachmentDataReader(HTMLAttachmentElement& attachment, Function<void(RefPtr<SharedBuffer>&&)>&& callback)
        : m_attachment(attachment)
        , m_callback(std::make_unique<Function<void(RefPtr<SharedBuffer>&&)>>(WTFMove(callback)))
        , m_loader(std::make_unique<FileReaderLoader>(FileReaderLoader::ReadType::ReadAsArrayBuffer, this))
    {
        m_loader->start(&attachment.document(), *attachment.file());
    }

    ~AttachmentDataReader();

private:
    void didStartLoading() final { }
    void didReceiveData() final { }
    void didFinishLoading() final;
    void didFail(int error) final;

    void invokeCallbackAndFinishReading(RefPtr<SharedBuffer>&&);

    HTMLAttachmentElement& m_attachment;
    std::unique_ptr<Function<void(RefPtr<SharedBuffer>&&)>> m_callback;
    std::unique_ptr<FileReaderLoader> m_loader;
};

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
    if (!style.hasAppearance()) {
        // If this attachment element doesn't have an appearance, defer rendering to child elements.
        return createRenderer<RenderBlockFlow>(*this, WTFMove(style));
    }

    return createRenderer<RenderAttachment>(*this, WTFMove(style));
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
            setAttributeWithoutSynchronization(HTMLNames::titleAttr, m_file->name());
            setAttributeWithoutSynchronization(HTMLNames::subtitleAttr, fileSizeDescription(m_file->size()));
            setAttributeWithoutSynchronization(HTMLNames::typeAttr, m_file->type());
        } else {
            removeAttribute(HTMLNames::titleAttr);
            removeAttribute(HTMLNames::subtitleAttr);
            removeAttribute(HTMLNames::typeAttr);
        }
    }

    if (auto* renderAttachment = attachmentRenderer())
        renderAttachment->invalidate();

    invalidateShadowRootChildrenIfNecessary();
    populateShadowRootIfNecessary();
}

void HTMLAttachmentElement::invalidateShadowRootChildrenIfNecessary()
{
    if (auto image = innerImage()) {
        image->setAttributeWithoutSynchronization(srcAttr, emptyString());
        image->setInlineStyleProperty(CSSPropertyDisplay, CSSValueNone, true);
    }
    if (auto video = innerVideo()) {
        video->setAttributeWithoutSynchronization(srcAttr, emptyString());
        video->setInlineStyleProperty(CSSPropertyDisplay, CSSValueNone, true);
    }
}

RenderAttachment* HTMLAttachmentElement::attachmentRenderer() const
{
    auto* renderer = this->renderer();
    return is<RenderAttachment>(renderer) ? downcast<RenderAttachment>(renderer) : nullptr;
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

void HTMLAttachmentElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == progressAttr || name == subtitleAttr || name == titleAttr || name == typeAttr) {
        if (auto* renderAttachment = attachmentRenderer())
            renderAttachment->invalidate();
    }

    HTMLElement::parseAttribute(name, value);
}

String HTMLAttachmentElement::uniqueIdentifier() const
{
    return attributeWithoutSynchronization(HTMLNames::webkitattachmentidAttr);
}

void HTMLAttachmentElement::setUniqueIdentifier(const String& identifier)
{
    setAttributeWithoutSynchronization(HTMLNames::webkitattachmentidAttr, identifier);
}

String HTMLAttachmentElement::attachmentTitle() const
{
    auto& title = attributeWithoutSynchronization(titleAttr);
    if (!title.isEmpty())
        return title;
    return m_file ? m_file->name() : String();
}

String HTMLAttachmentElement::attachmentType() const
{
    return attributeWithoutSynchronization(typeAttr);
}

String HTMLAttachmentElement::attachmentPath() const
{
    return attributeWithoutSynchronization(webkitattachmentpathAttr);
}

void HTMLAttachmentElement::updateDisplayMode(AttachmentDisplayMode mode)
{
    mode = mode == AttachmentDisplayMode::Auto ? defaultDisplayMode() : mode;

    switch (mode) {
    case AttachmentDisplayMode::InPlace:
        populateShadowRootIfNecessary();
        setInlineStyleProperty(CSSPropertyWebkitAppearance, CSSValueNone, true);
        setInlineStyleProperty(CSSPropertyDisplay, CSSValueInlineBlock, true);
        break;
    case AttachmentDisplayMode::AsIcon:
        removeInlineStyleProperty(CSSPropertyWebkitAppearance);
        removeInlineStyleProperty(CSSPropertyDisplay);
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }

    invalidateStyleAndRenderersForSubtree();
}

void HTMLAttachmentElement::updateFileWithData(Ref<SharedBuffer>&& data, std::optional<String>&& newContentType, std::optional<String>&& newFilename)
{
    auto filename = newFilename ? *newFilename : attachmentTitle();
    auto contentType = newContentType ? *newContentType : File::contentTypeForFile(filename);
    auto file = File::create(Blob::create(WTFMove(data), contentType), filename);
    setFile(WTFMove(file), UpdateDisplayAttributes::Yes);
}

Ref<HTMLImageElement> HTMLAttachmentElement::ensureInnerImage()
{
    if (auto image = innerImage())
        return *image;

    auto image = HTMLImageElement::create(document());
    ensureUserAgentShadowRoot().appendChild(image);
    return image;
}

Ref<HTMLVideoElement> HTMLAttachmentElement::ensureInnerVideo()
{
    if (auto video = innerVideo())
        return *video;

    auto video = HTMLVideoElement::create(document());
    ensureUserAgentShadowRoot().appendChild(video);
    return video;
}

RefPtr<HTMLImageElement> HTMLAttachmentElement::innerImage() const
{
    if (auto root = userAgentShadowRoot())
        return childrenOfType<HTMLImageElement>(*root).first();
    return nullptr;
}

RefPtr<HTMLVideoElement> HTMLAttachmentElement::innerVideo() const
{
    if (auto root = userAgentShadowRoot())
        return childrenOfType<HTMLVideoElement>(*root).first();
    return nullptr;
}

void HTMLAttachmentElement::populateShadowRootIfNecessary()
{
    if (!m_file)
        return;

    auto mimeType = attachmentType();

#if PLATFORM(COCOA)
    if (isDeclaredUTI(mimeType))
        mimeType = MIMETypeFromUTI(mimeType);
#endif

    if (mimeType.isEmpty())
        return;

    if (MIMETypeRegistry::isSupportedImageMIMEType(mimeType) || MIMETypeRegistry::isPDFMIMEType(mimeType)) {
        auto image = ensureInnerImage();
        if (image->attributeWithoutSynchronization(srcAttr).isEmpty()) {
            image->setAttributeWithoutSynchronization(srcAttr, DOMURL::createObjectURL(document(), *m_file));
            image->setInlineStyleProperty(CSSPropertyDisplay, CSSValueInline, true);
        }

    } else if (MIMETypeRegistry::isSupportedMediaMIMEType(mimeType)) {
        auto video = ensureInnerVideo();
        if (video->attributeWithoutSynchronization(srcAttr).isEmpty()) {
            video->setAttributeWithoutSynchronization(srcAttr, DOMURL::createObjectURL(document(), *m_file));
            video->setAttributeWithoutSynchronization(controlsAttr, emptyString());
            video->setInlineStyleProperty(CSSPropertyDisplay, CSSValueInline, true);
        }
    }
}

void HTMLAttachmentElement::requestData(Function<void(RefPtr<SharedBuffer>&&)>&& callback)
{
    if (m_file)
        m_attachmentReaders.append(AttachmentDataReader::create(*this, WTFMove(callback)));
    else
        callback(nullptr);
}

void HTMLAttachmentElement::destroyReader(AttachmentDataReader& finishedReader)
{
    m_attachmentReaders.removeFirstMatching([&] (const std::unique_ptr<AttachmentDataReader>& reader) -> bool {
        return reader.get() == &finishedReader;
    });
}

AttachmentDataReader::~AttachmentDataReader()
{
    invokeCallbackAndFinishReading(nullptr);
}

void AttachmentDataReader::didFinishLoading()
{
    if (auto arrayBuffer = m_loader->arrayBufferResult())
        invokeCallbackAndFinishReading(SharedBuffer::create(reinterpret_cast<uint8_t*>(arrayBuffer->data()), arrayBuffer->byteLength()));
    else
        invokeCallbackAndFinishReading(nullptr);
    m_attachment.destroyReader(*this);
}

void AttachmentDataReader::didFail(int)
{
    invokeCallbackAndFinishReading(nullptr);
    m_attachment.destroyReader(*this);
}

void AttachmentDataReader::invokeCallbackAndFinishReading(RefPtr<SharedBuffer>&& data)
{
    auto callback = WTFMove(m_callback);
    if (!callback)
        return;

    m_loader->cancel();
    m_loader = nullptr;
    (*callback)(WTFMove(data));
}

} // namespace WebCore

#endif // ENABLE(ATTACHMENT_ELEMENT)
