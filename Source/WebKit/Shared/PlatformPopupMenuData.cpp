/*
 * Copyright (C) 2010, 2011 Apple Inc. All rights reserved.
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
#include "PlatformPopupMenuData.h"

#include "WebCoreArgumentCoders.h"

namespace WebKit {

void PlatformPopupMenuData::encode(IPC::Encoder& encoder) const
{
#if PLATFORM(COCOA)
    encoder << fontInfo;
    encoder << shouldPopOver;
    encoder << hideArrows;
    encoder << menuSize;
#elif PLATFORM(WIN)
    encoder << m_clientPaddingLeft;
    encoder << m_clientPaddingRight;
    encoder << m_clientInsetLeft;
    encoder << m_clientInsetRight;
    encoder << m_popupWidth;
    encoder << m_itemHeight;

    ShareableBitmapHandle notSelectedBackingStoreHandle;
    m_notSelectedBackingStore->createHandle(notSelectedBackingStoreHandle);
    encoder << notSelectedBackingStoreHandle;

    ShareableBitmapHandle selectedBackingStoreHandle;
    m_selectedBackingStore->createHandle(selectedBackingStoreHandle);
    encoder << selectedBackingStoreHandle;
#else
    UNUSED_PARAM(encoder);
#endif
}

bool PlatformPopupMenuData::decode(IPC::Decoder& decoder, PlatformPopupMenuData& data)
{
#if PLATFORM(COCOA)
    if (!decoder.decode(data.fontInfo))
        return false;
    if (!decoder.decode(data.shouldPopOver))
        return false;
    if (!decoder.decode(data.hideArrows))
        return false;
    if (!decoder.decode(data.menuSize))
        return false;
#elif PLATFORM(WIN)
    if (!decoder.decode(data.m_clientPaddingLeft))
        return false;
    if (!decoder.decode(data.m_clientPaddingRight))
        return false;
    if (!decoder.decode(data.m_clientInsetLeft))
        return false;
    if (!decoder.decode(data.m_clientInsetRight))
        return false;
    if (!decoder.decode(data.m_popupWidth))
        return false;
    if (!decoder.decode(data.m_itemHeight))
        return false;

    ShareableBitmapHandle notSelectedBackingStoreHandle;
    if (!decoder.decode(notSelectedBackingStoreHandle))
        return false;
    data.m_notSelectedBackingStore = ShareableBitmap::create(notSelectedBackingStoreHandle);

    ShareableBitmapHandle selectedBackingStoreHandle;
    if (!decoder.decode(selectedBackingStoreHandle))
        return false;
    data.m_selectedBackingStore = ShareableBitmap::create(selectedBackingStoreHandle);
#else
    UNUSED_PARAM(decoder);
    UNUSED_PARAM(data);
#endif
    
    return true;
}

std::optional<PlatformPopupMenuData> PlatformPopupMenuData::decode(IPC::Decoder& decoder)
{
    PlatformPopupMenuData data;
    if (!decode(decoder, data))
        return std::nullopt;
    return data;
}

} // namespace WebKit
