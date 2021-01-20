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
#include <WebCore/ContextMenuContext.h>
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

ContextMenuContextData::ContextMenuContextData(const WebCore::IntPoint& menuLocation, const Vector<WebKit::WebContextMenuItemData>& menuItems, const ContextMenuContext& context)
#if ENABLE(SERVICE_CONTROLS)
    : m_type(context.controlledImage() ? Type::ServicesMenu : Type::ContextMenu)
#else
    : m_type(Type::ContextMenu)
#endif
    , m_menuLocation(menuLocation)
    , m_menuItems(menuItems)
    , m_webHitTestResultData(context.hitTestResult(), true)
    , m_selectedText(context.selectedText())
#if ENABLE(SERVICE_CONTROLS)
    , m_selectionIsEditable(false)
#endif
{
#if ENABLE(SERVICE_CONTROLS)
    Image* image = context.controlledImage();
    if (!image)
        return;

    // FIXME: figure out the rounding strategy for ShareableBitmap.
    m_controlledImage = ShareableBitmap::createShareable(IntSize(image->size()), { });
    auto graphicsContext = m_controlledImage->createGraphicsContext();
    if (!graphicsContext)
        return;
    graphicsContext->drawImage(*image, IntPoint());
#endif
}

void ContextMenuContextData::encode(IPC::Encoder& encoder) const
{
    encoder << m_type;
    encoder << m_menuLocation;
    encoder << m_menuItems;
    encoder << m_webHitTestResultData;
    encoder << m_selectedText;

#if ENABLE(SERVICE_CONTROLS)
    ShareableBitmap::Handle handle;
    if (m_controlledImage)
        m_controlledImage->createHandle(handle, SharedMemory::Protection::ReadOnly);
    encoder << handle;
    encoder << m_controlledSelectionData;
    encoder << m_selectedTelephoneNumbers;
    encoder << m_selectionIsEditable;
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

#if ENABLE(SERVICE_CONTROLS)
    ShareableBitmap::Handle handle;
    if (!decoder.decode(handle))
        return false;

    if (!handle.isNull())
        result.m_controlledImage = ShareableBitmap::create(handle, SharedMemory::Protection::ReadOnly);

    if (!decoder.decode(result.m_controlledSelectionData))
        return false;
    if (!decoder.decode(result.m_selectedTelephoneNumbers))
        return false;
    if (!decoder.decode(result.m_selectionIsEditable))
        return false;
#endif

    return true;
}

#if ENABLE(SERVICE_CONTROLS)
bool ContextMenuContextData::controlledDataIsEditable() const
{
    if (!m_controlledSelectionData.isEmpty())
        return m_selectionIsEditable;

    if (m_controlledImage)
        return m_webHitTestResultData.isContentEditable;

    return false;
}
#endif

} // namespace WebKit

#endif // ENABLE(CONTEXT_MENUS)
