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

PlatformPopupMenuData::PlatformPopupMenuData()
#if PLATFORM(WIN)
    : m_clientPaddingLeft(0)
    , m_clientPaddingRight(0)
    , m_clientInsetLeft(0)
    , m_clientInsetRight(0)
    , m_popupWidth(0)
    , m_itemHeight(0)
#endif
{
}

void PlatformPopupMenuData::encode(CoreIPC::ArgumentEncoder* encoder) const
{
#if PLATFORM(WIN)
    encoder->encode(m_clientPaddingLeft);
    encoder->encode(m_clientPaddingRight);
    encoder->encode(m_clientInsetLeft);
    encoder->encode(m_clientInsetRight);
    encoder->encode(m_popupWidth);
    encoder->encode(m_itemHeight);

    ShareableBitmap::Handle notSelectedBackingStoreHandle;
    m_notSelectedBackingStore->createHandle(notSelectedBackingStoreHandle);
    encoder->encode(notSelectedBackingStoreHandle);

    ShareableBitmap::Handle selectedBackingStoreHandle;
    m_selectedBackingStore->createHandle(selectedBackingStoreHandle);
    encoder->encode(selectedBackingStoreHandle);
#elif PLATFORM(MAC)
    encoder->encode(fontInfo);
    encoder->encode(shouldPopOver);
#endif
}

bool PlatformPopupMenuData::decode(CoreIPC::ArgumentDecoder* decoder, PlatformPopupMenuData& data)
{
#if PLATFORM(WIN)
    if (!decoder->decode(data.m_clientPaddingLeft))
        return false;
    if (!decoder->decode(data.m_clientPaddingRight))
        return false;
    if (!decoder->decode(data.m_clientInsetLeft))
        return false;
    if (!decoder->decode(data.m_clientInsetRight))
        return false;
    if (!decoder->decode(data.m_popupWidth))
        return false;
    if (!decoder->decode(data.m_itemHeight))
        return false;

    ShareableBitmap::Handle notSelectedBackingStoreHandle;
    if (!decoder->decode(notSelectedBackingStoreHandle))
        return false;
    data.m_notSelectedBackingStore = ShareableBitmap::create(notSelectedBackingStoreHandle);

    ShareableBitmap::Handle selectedBackingStoreHandle;
    if (!decoder->decode(selectedBackingStoreHandle))
        return false;
    data.m_selectedBackingStore = ShareableBitmap::create(selectedBackingStoreHandle);
#elif PLATFORM(MAC)
    if (!decoder->decode(data.fontInfo))
        return false;
    if (!decoder->decode(data.shouldPopOver))
        return false;
#endif
    
    return true;
}

} // namespace WebKit
