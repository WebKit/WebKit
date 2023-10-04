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

#include "WebCoreArgumentCoders.h"
#include <WebCore/GraphicsContext.h>

namespace WebKit {
using namespace WebCore;

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
#if ENABLE(SERVICE_CONTROLS)
    , m_selectionIsEditable(false)
#endif
{
#if ENABLE(SERVICE_CONTROLS)
    if (auto* image = context.controlledImage())
        setImage(*image);
#endif
#if ENABLE(CONTEXT_MENU_QR_CODE_DETECTION)
    if (auto* image = context.potentialQRCodeNodeSnapshotImage())
        setPotentialQRCodeNodeSnapshotImage(*image);

    if (auto* image = context.potentialQRCodeViewportSnapshotImage())
        setPotentialQRCodeViewportSnapshotImage(*image);
#endif
}

#if ENABLE(SERVICE_CONTROLS)
ContextMenuContextData::ContextMenuContextData(const WebCore::IntPoint& menuLocation, WebCore::Image& image, bool isEditable, const WebCore::IntRect& imageRect, const String& attachmentID, std::optional<ElementContext>&& elementContext, const String& sourceImageMIMEType)
    : m_type(Type::ServicesMenu)
    , m_menuLocation(menuLocation)
    , m_selectionIsEditable(isEditable)
    , m_controlledImageBounds(imageRect)
    , m_controlledImageAttachmentID(attachmentID)
    , m_controlledImageElementContext(WTFMove(elementContext))
    , m_controlledImageMIMEType(sourceImageMIMEType)
{
    setImage(image);
}

void ContextMenuContextData::setImage(WebCore::Image& image)
{
    // FIXME: figure out the rounding strategy for ShareableBitmap.
    m_controlledImage = ShareableBitmap::create({ IntSize(image.size()) });
    if (auto graphicsContext = m_controlledImage->createGraphicsContext())
        graphicsContext->drawImage(image, IntPoint());
}
#endif

#if ENABLE(CONTEXT_MENU_QR_CODE_DETECTION)

void ContextMenuContextData::setPotentialQRCodeNodeSnapshotImage(WebCore::Image& image)
{
    m_potentialQRCodeNodeSnapshotImage = ShareableBitmap::create({ IntSize(image.size()) });
    if (auto graphicsContext = m_potentialQRCodeNodeSnapshotImage->createGraphicsContext())
        graphicsContext->drawImage(image, IntPoint());
}

void ContextMenuContextData::setPotentialQRCodeViewportSnapshotImage(WebCore::Image& image)
{
    m_potentialQRCodeViewportSnapshotImage = ShareableBitmap::create({ IntSize(image.size()) });
    if (auto graphicsContext = m_potentialQRCodeViewportSnapshotImage->createGraphicsContext())
        graphicsContext->drawImage(image, IntPoint());
}

#endif // ENABLE(CONTEXT_MENU_QR_CODE_DETECTION)

void ContextMenuContextData::encode(IPC::Encoder& encoder) const
{
    encoder << m_type;
    encoder << m_menuLocation;
    encoder << m_menuItems;
    encoder << m_webHitTestResultData;
    encoder << m_selectedText;
    encoder << m_hasEntireImage;

#if ENABLE(SERVICE_CONTROLS)
    std::optional<ShareableBitmap::Handle> potentialControlledImageHandle;
    if (m_controlledImage)
        potentialControlledImageHandle = m_controlledImage->createHandle(SharedMemory::Protection::ReadOnly);

    encoder << WTFMove(potentialControlledImageHandle);
    encoder << m_controlledSelectionData;
    encoder << m_selectedTelephoneNumbers;
    encoder << m_selectionIsEditable;
    encoder << m_controlledImageBounds;
    encoder << m_controlledImageAttachmentID;
    encoder << m_controlledImageElementContext;
    encoder << m_controlledImageMIMEType;
#endif

#if ENABLE(CONTEXT_MENU_QR_CODE_DETECTION)
    std::optional<ShareableBitmap::Handle> potentialQRCodeNodeSnapshotImageHandle;
    if (m_potentialQRCodeNodeSnapshotImage) {
        potentialQRCodeNodeSnapshotImageHandle = m_potentialQRCodeNodeSnapshotImage->createHandle(SharedMemory::Protection::ReadOnly);
    }
    encoder << WTFMove(potentialQRCodeNodeSnapshotImageHandle);

    std::optional<ShareableBitmap::Handle> potentialQRCodeViewportSnapshotImageHandle;
    if (m_potentialQRCodeViewportSnapshotImage) {
        potentialQRCodeViewportSnapshotImageHandle = m_potentialQRCodeViewportSnapshotImage->createHandle(SharedMemory::Protection::ReadOnly);
    }
    encoder << WTFMove(potentialQRCodeViewportSnapshotImageHandle);
#endif
}

bool ContextMenuContextData::decode(IPC::Decoder& decoder, ContextMenuContextData& result)
{
    if (!decoder.decode(result.m_type))
        return false;

    if (!decoder.decode(result.m_menuLocation))
        return false;

    if (!decoder.decode(result.m_menuItems))
        return false;

    if (!decoder.decode(result.m_webHitTestResultData))
        return false;

    if (!decoder.decode(result.m_selectedText))
        return false;

    if (!decoder.decode(result.m_hasEntireImage))
        return false;

#if ENABLE(SERVICE_CONTROLS)
    auto potentialControlledImageHandle = decoder.decode<std::optional<ShareableBitmap::Handle>>();
    if (UNLIKELY(!decoder.isValid()))
        return false;
    if (*potentialControlledImageHandle)
        result.m_controlledImage = ShareableBitmap::create(WTFMove(**potentialControlledImageHandle), SharedMemory::Protection::ReadOnly);

    if (!decoder.decode(result.m_controlledSelectionData))
        return false;
    if (!decoder.decode(result.m_selectedTelephoneNumbers))
        return false;
    if (!decoder.decode(result.m_selectionIsEditable))
        return false;
    if (!decoder.decode(result.m_controlledImageBounds))
        return false;
    if (!decoder.decode(result.m_controlledImageAttachmentID))
        return false;
    if (!decoder.decode(result.m_controlledImageElementContext))
        return false;
    if (!decoder.decode(result.m_controlledImageMIMEType))
        return false;
#endif

#if ENABLE(CONTEXT_MENU_QR_CODE_DETECTION)
    auto potentialQRCodeNodeSnapshotImageHandle = decoder.decode<std::optional<ShareableBitmap::Handle>>();
    auto potentialQRCodeViewportSnapshotImageHandle = decoder.decode<std::optional<ShareableBitmap::Handle>>();
    if (UNLIKELY(!decoder.isValid()))
        return false;
    if (*potentialQRCodeNodeSnapshotImageHandle)
        result.m_potentialQRCodeNodeSnapshotImage = ShareableBitmap::create(WTFMove(**potentialQRCodeNodeSnapshotImageHandle), SharedMemory::Protection::ReadOnly);
    if (*potentialQRCodeViewportSnapshotImageHandle)
        result.m_potentialQRCodeViewportSnapshotImage = ShareableBitmap::create(WTFMove(**potentialQRCodeViewportSnapshotImageHandle), SharedMemory::Protection::ReadOnly);
#endif

    return true;
}

#if ENABLE(SERVICE_CONTROLS)
bool ContextMenuContextData::controlledDataIsEditable() const
{
    if (!m_controlledSelectionData.isEmpty() || m_controlledImage || !m_controlledImageAttachmentID.isNull())
        return m_selectionIsEditable;

    return false;
}
#endif

} // namespace WebKit

#endif // ENABLE(CONTEXT_MENUS)
