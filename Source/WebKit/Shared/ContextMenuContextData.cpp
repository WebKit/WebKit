/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE, INC. ``AS IS'' AND ANY
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
 *
 */

#include "config.h"
#include "ContextMenuContextData.h"

#if ENABLE(CONTEXT_MENUS)

#include "WebEventConversion.h"
#include <WebCore/GraphicsContext.h>
#include <WebCore/MouseEvent.h>

namespace WebKit {

ContextMenuContextData::ContextMenuContextData()
    : m_type(Type::ContextMenu)
#if ENABLE(SERVICE_CONTROLS)
    , m_selectionIsEditable(false)
#endif
{
}

ContextMenuContextData::ContextMenuContextData(const IntPoint& menuLocation, const Vector<WebKit::WebContextMenuItemData>& menuItems, const ContextMenuContext& context)
#if ENABLE(SERVICE_CONTROLS)
    : m_type(context.controlledImage() ? Type::ServicesMenu : context.type())
#else
    : m_type(context.type())
#endif
    , m_menuLocation(menuLocation)
    , m_menuItems(menuItems)
    , m_webHitTestResultData({ context.hitTestResult(), true })
    , m_selectedText(context.selectedText())
    , m_hasEntireImage(context.hasEntireImage())
    , m_allowsFollowingLink(context.allowsFollowingLink())
    , m_allowsFollowingImageURL(context.allowsFollowingImageURL())
#if ENABLE(SERVICE_CONTROLS)
    , m_selectionIsEditable(false)
#endif
{
#if ENABLE(SERVICE_CONTROLS)
    if (RefPtr image = context.controlledImage())
        setImage(*image);
#endif
#if ENABLE(CONTEXT_MENU_QR_CODE_DETECTION)
    if (RefPtr image = context.potentialQRCodeNodeSnapshotImage())
        setPotentialQRCodeNodeSnapshotImage(*image);

    if (RefPtr image = context.potentialQRCodeViewportSnapshotImage())
        setPotentialQRCodeViewportSnapshotImage(*image);
#endif

#if ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS)
    if (auto identifier = context.mediaElementIdentifier())
        setMediaElementIdentifier(*identifier);
#endif

    if (RefPtr event = dynamicDowncast<WebCore::MouseEvent>(context.event())) {
        m_inputSource = event->inputSource()
            .transform([](auto& inputSource) {
                return kit(inputSource);
            });
    }
}

#if ENABLE(SERVICE_CONTROLS)
ContextMenuContextData::ContextMenuContextData(const WebCore::IntPoint& menuLocation, WebCore::Image& image, bool isEditable, const WebCore::IntRect& imageRect, const String& attachmentID, std::optional<ElementContext>&& elementContext, const String& sourceImageMIMEType)
    : m_type(Type::ServicesMenu)
    , m_menuLocation(menuLocation)
    , m_selectionIsEditable(isEditable)
    , m_controlledImageBounds(imageRect)
    , m_controlledImageAttachmentID(attachmentID)
    , m_controlledImageElementContext(WTF::move(elementContext))
    , m_controlledImageMIMEType(sourceImageMIMEType)
{
    setImage(image);
}

void ContextMenuContextData::setImage(WebCore::Image& image)
{
    // FIXME: figure out the rounding strategy for ShareableBitmap.

    RefPtr controlledImage = ShareableBitmap::create({ IntSize(image.size()) });
    m_controlledImage = controlledImage;
    if (auto graphicsContext = controlledImage->createGraphicsContext())
        graphicsContext->drawImage(image, IntPoint());
}

std::optional<ShareableBitmap::Handle> ContextMenuContextData::createControlledImageReadOnlyHandle() const
{
    if (RefPtr controlledImage = m_controlledImage)
        return controlledImage->createHandle(SharedMemory::Protection::ReadOnly);
    return std::nullopt;
}
#endif

#if ENABLE(CONTEXT_MENU_QR_CODE_DETECTION)

void ContextMenuContextData::setPotentialQRCodeNodeSnapshotImage(WebCore::Image& image)
{
    RefPtr potentialQRCodeNodeSnapshotImage = ShareableBitmap::create({ IntSize(image.size()) });
    m_potentialQRCodeNodeSnapshotImage = potentialQRCodeNodeSnapshotImage;
    if (auto graphicsContext = potentialQRCodeNodeSnapshotImage->createGraphicsContext())
        graphicsContext->drawImage(image, IntPoint());
}

void ContextMenuContextData::setPotentialQRCodeViewportSnapshotImage(WebCore::Image& image)
{
    RefPtr potentialQRCodeViewportSnapshotImage = ShareableBitmap::create({ IntSize(image.size()) });
    m_potentialQRCodeViewportSnapshotImage = potentialQRCodeViewportSnapshotImage;
    if (auto graphicsContext = potentialQRCodeViewportSnapshotImage->createGraphicsContext())
        graphicsContext->drawImage(image, IntPoint());
}

std::optional<ShareableBitmap::Handle> ContextMenuContextData::createPotentialQRCodeNodeSnapshotImageReadOnlyHandle() const
{
    if (RefPtr image = m_potentialQRCodeNodeSnapshotImage)
        return image->createHandle(SharedMemory::Protection::ReadOnly);
    return std::nullopt;
}

std::optional<ShareableBitmap::Handle> ContextMenuContextData::createPotentialQRCodeViewportSnapshotImageReadOnlyHandle() const
{
    if (RefPtr image = m_potentialQRCodeViewportSnapshotImage)
        return image->createHandle(SharedMemory::Protection::ReadOnly);
    return std::nullopt;
}

#endif // ENABLE(CONTEXT_MENU_QR_CODE_DETECTION)

ContextMenuContextData::ContextMenuContextData(WebCore::ContextMenuContext::Type type
    , WebCore::IntPoint&& menuLocation
    , Vector<WebContextMenuItemData>&& menuItems
    , std::optional<WebKit::WebHitTestResultData>&& webHitTestResultData
    , String&& selectedText
#if ENABLE(SERVICE_CONTROLS)
    , std::optional<WebCore::ShareableBitmapHandle>&& controlledImageHandle
    , WebCore::AttributedString&& controlledSelection
    , Vector<String>&& selectedTelephoneNumbers
    , bool selectionIsEditable
    , WebCore::IntRect&& controlledImageBounds
    , String&& controlledImageAttachmentID
    , std::optional<WebCore::ElementContext>&& controlledImageElementContext
    , String&& controlledImageMIMEType
#endif // ENABLE(SERVICE_CONTROLS)
#if ENABLE(CONTEXT_MENU_QR_CODE_DETECTION)
    , std::optional<WebCore::ShareableBitmapHandle>&& potentialQRCodeNodeSnapshotImageHandle
    , std::optional<WebCore::ShareableBitmapHandle>&& potentialQRCodeViewportSnapshotImageHandle
#endif // ENABLE(CONTEXT_MENU_QR_CODE_DETECTION)
    , bool hasEntireImage
    , bool allowsFollowingLink
    , bool allowsFollowingImageURL
#if ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS)
    , std::optional<WebCore::HTMLMediaElementIdentifier> mediaElementIdentifier
#endif
    , std::optional<WebMouseEventInputSource> inputSource
)
    : m_type(type)
    , m_menuLocation(WTF::move(menuLocation))
    , m_menuItems(WTF::move(menuItems))
    , m_webHitTestResultData(WTF::move(webHitTestResultData))
    , m_selectedText(WTF::move(selectedText))
    , m_hasEntireImage(hasEntireImage)
    , m_allowsFollowingLink(allowsFollowingLink)
    , m_allowsFollowingImageURL(allowsFollowingImageURL)
#if ENABLE(SERVICE_CONTROLS)
    , m_controlledSelection(WTF::move(controlledSelection))
    , m_selectedTelephoneNumbers(WTF::move(selectedTelephoneNumbers))
    , m_selectionIsEditable(selectionIsEditable)
    , m_controlledImageBounds(WTF::move(controlledImageBounds))
    , m_controlledImageAttachmentID(WTF::move(controlledImageAttachmentID))
    , m_controlledImageElementContext(WTF::move(controlledImageElementContext))
    , m_controlledImageMIMEType(WTF::move(controlledImageMIMEType))
#endif
#if ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS)
    , m_mediaElementIdentifier(WTF::move(mediaElementIdentifier))
#endif
    , m_inputSource(inputSource)
{
#if ENABLE(SERVICE_CONTROLS)
    if (controlledImageHandle)
        m_controlledImage = ShareableBitmap::create(WTF::move(*controlledImageHandle), SharedMemory::Protection::ReadOnly);
#endif // ENABLE(SERVICE_CONTROLS)

#if ENABLE(CONTEXT_MENU_QR_CODE_DETECTION)
    if (potentialQRCodeNodeSnapshotImageHandle)
        m_potentialQRCodeNodeSnapshotImage = ShareableBitmap::create(WTF::move(*potentialQRCodeNodeSnapshotImageHandle), SharedMemory::Protection::ReadOnly);
    if (potentialQRCodeViewportSnapshotImageHandle)
        m_potentialQRCodeViewportSnapshotImage = ShareableBitmap::create(WTF::move(*potentialQRCodeViewportSnapshotImageHandle), SharedMemory::Protection::ReadOnly);
#endif
}

#if ENABLE(SERVICE_CONTROLS)
bool ContextMenuContextData::controlledDataIsEditable() const
{
    if (!m_controlledSelection.isNull() || m_controlledImage || !m_controlledImageAttachmentID.isNull())
        return m_selectionIsEditable;

    return false;
}
#endif

} // namespace WebKit

#endif // ENABLE(CONTEXT_MENUS)
