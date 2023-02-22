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

#include "AddEventListenerOptions.h"
#include "AttachmentElementClient.h"
#include "CustomEvent.h"
#include "DOMURL.h"
#include "Document.h"
#include "Editor.h"
#include "ElementInlines.h"
#include "EventNames.h"
#include "File.h"
#include "Frame.h"
#include "HTMLButtonElement.h"
#include "HTMLDivElement.h"
#include "HTMLElementTypeHelpers.h"
#include "HTMLImageElement.h"
#include "HTMLNames.h"
#include "HTMLStyleElement.h"
#include "MIMETypeRegistry.h"
#include "RenderAttachment.h"
#include "ShadowRoot.h"
#include "SharedBuffer.h"
#include "UserAgentStyleSheets.h"
#include <pal/FileSizeFormatter.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/UUID.h>
#include <wtf/URLParser.h>
#include <wtf/unicode/CharacterNames.h>

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
    Ref attachment = adoptRef(*new HTMLAttachmentElement(tagName, document));
    if (document.settings().attachmentWideLayoutEnabled()) {
        ASSERT(attachment->m_implementation == Implementation::Legacy);
        ASSERT(!attachment->renderer()); // Switch to modern style *must* be done before renderer is created!
        attachment->m_implementation = Implementation::Modern;
        attachment->ensureUserAgentShadowRoot();
    }
    return attachment;
}

void HTMLAttachmentElement::didAddUserAgentShadowRoot(ShadowRoot& root)
{
    if (m_implementation == Implementation::Modern)
        ensureModernShadowTree(root);
}

static const AtomString& attachmentContainerIdentifier()
{
    static MainThreadNeverDestroyed<const AtomString> identifier("attachment-container"_s);
    return identifier;
}

static const AtomString& attachmentPreviewIdentifier()
{
    static MainThreadNeverDestroyed<const AtomString> identifier("attachment-preview"_s);
    return identifier;
}

static const AtomString& attachmentActionIdentifier()
{
    static MainThreadNeverDestroyed<const AtomString> identifier("attachment-action"_s);
    return identifier;
}

static const AtomString& attachmentTitleIdentifier()
{
    static MainThreadNeverDestroyed<const AtomString> identifier("attachment-title"_s);
    return identifier;
}

static const AtomString& attachmentSubtitleIdentifier()
{
    static MainThreadNeverDestroyed<const AtomString> identifier("attachment-subtitle"_s);
    return identifier;
}

static const AtomString& attachmentSaveButtonIdentifier()
{
    static MainThreadNeverDestroyed<const AtomString> identifier("attachment-save-button"_s);
    return identifier;
}

static QualifiedName subtitleAttr()
{
    return QualifiedName { nullAtom(), "subtitle"_s, nullAtom() };
}

static QualifiedName saveAttr()
{
    return QualifiedName { nullAtom(), "save"_s, nullAtom() };
}

template <typename ElementType>
static Ref<ElementType> createContainedElement(HTMLElement& container, const AtomString& id, String textContent)
{
    Ref<ElementType> element = ElementType::create(container.document());
    element->setIdAttribute(id);
    if (!textContent.isEmpty())
        element->setTextContent(WTFMove(textContent));
    container.appendChild(element);
    return element;
}

void HTMLAttachmentElement::ensureModernShadowTree(ShadowRoot& root)
{
    ASSERT(m_implementation == Implementation::Modern);
    if (m_titleElement)
        return;

    static MainThreadNeverDestroyed<const String> shadowStyle(StringImpl::createWithoutCopying(attachmentElementShadowUserAgentStyleSheet, sizeof(attachmentElementShadowUserAgentStyleSheet)));
    auto style = HTMLStyleElement::create(HTMLNames::styleTag, document(), false);
    style->setTextContent(String { shadowStyle });
    root.appendChild(WTFMove(style));

    m_containerElement = HTMLDivElement::create(document());
    m_containerElement->setIdAttribute(attachmentContainerIdentifier());
    root.appendChild(*m_containerElement);

    // FIXME: This is using the same HTMLAttachmentElement type, but with different behavior (thanks to m_implementation), to fetch and show
    // the appropriate image (thumbnail, icon, etc.). In the longer term, this functionality should be folded into the Implementation::Modern
    // code, and the old Legacy/ImageOnly code should be removed; this element could be an image (with a different data member name). See rdar://105252742.
    m_innerLegacyAttachment = adoptRef(*new HTMLAttachmentElement(HTMLNames::attachmentTag, document()));
    m_innerLegacyAttachment->m_implementation = Implementation::ImageOnly;
    m_innerLegacyAttachment->cloneAttributesFromElement(*this);
    m_innerLegacyAttachment->m_file = m_file;
    m_innerLegacyAttachment->m_thumbnail = WTFMove(m_thumbnail);
    m_innerLegacyAttachment->m_icon = WTFMove(m_icon);
    m_innerLegacyAttachment->m_iconSize = m_iconSize;
    m_innerLegacyAttachment->setIdAttribute(attachmentPreviewIdentifier());
    m_containerElement->appendChild(*m_innerLegacyAttachment);

    m_actionTextElement = createContainedElement<HTMLDivElement>(*m_containerElement, attachmentActionIdentifier(), attachmentActionForDisplay());

    m_titleElement = createContainedElement<HTMLDivElement>(*m_containerElement, attachmentTitleIdentifier(), attachmentTitleForDisplay());

    m_subtitleElement = createContainedElement<HTMLDivElement>(*m_containerElement, attachmentSubtitleIdentifier(), attachmentSubtitleForDisplay());

    updateSaveButton(attributeWithoutSynchronization(saveAttr()));
}

class AttachmentSaveEventListener final : public EventListener {
public:
    static Ref<AttachmentSaveEventListener> create(HTMLAttachmentElement& attachment) { return adoptRef(*new AttachmentSaveEventListener(attachment)); }

    bool operator==(const EventListener& other) const final
    {
        return this == &other;
    }

    void handleEvent(ScriptExecutionContext&, Event& event) final
    {
        if (event.type() == eventNames().clickEvent) {
            event.preventDefault();
            event.stopPropagation();
            event.stopImmediatePropagation();

            CustomEvent::Init init;
            init.bubbles = true;
            init.cancelable = true;
            init.composed = true;
            m_attachment->dispatchEvent(CustomEvent::create(m_attachment->attributeWithoutSynchronization(saveAttr()), init, event.isTrusted() ? Event::IsTrusted::Yes : Event::IsTrusted::No));
        } else
            ASSERT_NOT_REACHED();
    }

private:
    explicit AttachmentSaveEventListener(HTMLAttachmentElement& attachment)
        : EventListener(CPPEventListenerType)
        , m_attachment(attachment)
    {
    }

    WeakPtr<HTMLAttachmentElement, WeakPtrImplWithEventTargetData> m_attachment;
};

void HTMLAttachmentElement::updateSaveButton(const AtomString& eventTypeName)
{
    if (!m_containerElement)
        return;

    if (eventTypeName.isNull()) {
        if (m_saveButton) {
            m_containerElement->removeChild(*m_saveButton);
            m_saveButton = nullptr;
        }
        return;
    }

    if (!m_saveButton) {
        m_saveButton = createContainedElement<HTMLButtonElement>(*m_containerElement, attachmentSaveButtonIdentifier(), String { });
        m_saveButton->addEventListener(eventNames().clickEvent, AttachmentSaveEventListener::create(*this), { });
    }
}


RenderPtr<RenderElement> HTMLAttachmentElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition& position)
{
    if (m_implementation == Implementation::Modern)
        return HTMLElement::createElementRenderer(WTFMove(style), position);

    return createRenderer<RenderAttachment>(*this, WTFMove(style));
}

void HTMLAttachmentElement::invalidateRendering()
{
    if (auto* renderer = this->renderer()) {
        renderer->setNeedsLayout();
        renderer->repaint();
    }
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
            setAttributeWithoutSynchronization(subtitleAttr(), PAL::fileSizeDescription(m_file->size()));
            setAttributeWithoutSynchronization(HTMLNames::typeAttr, AtomString { m_file->type() });
        } else {
            removeAttribute(HTMLNames::titleAttr);
            removeAttribute(subtitleAttr());
            removeAttribute(HTMLNames::typeAttr);
        }
    }

    invalidateRendering();
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
    if (name == actionAttr || name == progressAttr || name == subtitleAttr() || name == titleAttr || name == typeAttr)
        invalidateRendering();

    HTMLElement::parseAttribute(name, value);

    if (name == actionAttr) {
        if (m_actionTextElement)
            m_actionTextElement->setTextContent(String(value.string()));
    } else if (name == titleAttr) {
        if (m_titleElement)
            m_titleElement->setTextContent(String(value.string()));
    } else if (name == subtitleAttr()) {
        if (m_subtitleElement)
            m_subtitleElement->setTextContent(String(value.string()));
    } else if (name == saveAttr())
        updateSaveButton(value);

    if (m_innerLegacyAttachment)
        m_innerLegacyAttachment->setAttributeWithoutSynchronization(name, value);

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

const AtomString& HTMLAttachmentElement::attachmentSubtitle() const
{
    return attributeWithoutSynchronization(subtitleAttr());
}

const AtomString& HTMLAttachmentElement::attachmentActionForDisplay() const
{
    if (m_implementation == Implementation::ImageOnly)
        return nullAtom();

    return attributeWithoutSynchronization(actionAttr);
}

String HTMLAttachmentElement::attachmentTitleForDisplay() const
{
    if (m_implementation == Implementation::ImageOnly)
        return { };

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

const AtomString& HTMLAttachmentElement::attachmentSubtitleForDisplay() const
{
    if (m_implementation == Implementation::ImageOnly)
        return nullAtom();

    return attachmentSubtitle();
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
        setAttributeWithoutSynchronization(subtitleAttr(), PAL::fileSizeDescription(*newFileSize));
    else
        removeAttribute(subtitleAttr());

    invalidateRendering();
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
    invalidateRendering();
}

void HTMLAttachmentElement::updateIcon(const RefPtr<Image>& icon, const WebCore::FloatSize& iconSize)
{
    m_icon = icon;
    m_iconSize = iconSize;
    invalidateRendering();
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
