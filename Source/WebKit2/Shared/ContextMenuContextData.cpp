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

using namespace WebCore;

namespace WebKit {

ContextMenuContextData::ContextMenuContextData()
{
}

ContextMenuContextData::ContextMenuContextData(const ContextMenuContext& context)
    : m_webHitTestResultData(WebHitTestResult::Data(context.hitTestResult()))
{
#if ENABLE(IMAGE_CONTROLS)
    Image* image = context.controlledImage();
    if (!image)
        return;

    RefPtr<ShareableBitmap> bitmap = ShareableBitmap::createShareable(image->size(), ShareableBitmap::SupportsAlpha);
    bitmap->createGraphicsContext()->drawImage(image, ColorSpaceDeviceRGB, IntPoint());
    bitmap->createHandle(m_controlledImageHandle);
#endif
}

ContextMenuContextData::ContextMenuContextData(const ContextMenuContextData& other)
{
    *this = other;
}

ContextMenuContextData& ContextMenuContextData::operator=(const ContextMenuContextData& other)
{
    m_webHitTestResultData = other.m_webHitTestResultData;
#if ENABLE(IMAGE_CONTROLS)
    m_controlledImageHandle.clear();

    if (!other.m_controlledImageHandle.isNull()) {
        RefPtr<ShareableBitmap> bitmap = ShareableBitmap::create(other.m_controlledImageHandle);
        bitmap->createHandle(m_controlledImageHandle);
    }
#endif

    return *this;
}

void ContextMenuContextData::encode(IPC::ArgumentEncoder& encoder) const
{
    encoder << m_webHitTestResultData;
#if ENABLE(IMAGE_CONTROLS)
    encoder << m_controlledImageHandle;
#endif
}

bool ContextMenuContextData::decode(IPC::ArgumentDecoder& decoder, ContextMenuContextData& contextMenuContextData)
{
    if (!decoder.decode(contextMenuContextData.m_webHitTestResultData))
        return false;
        
#if ENABLE(IMAGE_CONTROLS)
    if (!decoder.decode(contextMenuContextData.m_controlledImageHandle))
        return false;
#endif

    return true;
}

} // namespace WebKit

#endif // ENABLE(CONTEXT_MENUS)
