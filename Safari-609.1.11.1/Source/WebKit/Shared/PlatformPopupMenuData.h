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

#pragma once

#include "FontInfo.h"
#include "ShareableBitmap.h"
#include <WebCore/PopupMenuStyle.h>
#include <wtf/text/WTFString.h>

namespace IPC {
class Decoder;
class Encoder;
}

namespace WebKit {

struct PlatformPopupMenuData {
    PlatformPopupMenuData() = default;

    void encode(IPC::Encoder&) const;
    static bool decode(IPC::Decoder&, PlatformPopupMenuData&);
    static Optional<PlatformPopupMenuData> decode(IPC::Decoder&);

#if PLATFORM(COCOA)
    FontInfo fontInfo;
    bool shouldPopOver { false };
    bool hideArrows { false };
    WebCore::PopupMenuStyle::PopupMenuSize menuSize { WebCore::PopupMenuStyle::PopupMenuSize::PopupMenuSizeNormal };
#elif PLATFORM(WIN)
    int m_clientPaddingLeft { 0 };
    int m_clientPaddingRight { 0 };
    int m_clientInsetLeft { 0 };
    int m_clientInsetRight { 0 };
    int m_popupWidth { 0 };
    int m_itemHeight { 0 };
    RefPtr<ShareableBitmap> m_notSelectedBackingStore;
    RefPtr<ShareableBitmap> m_selectedBackingStore;
#endif
};

} // namespace WebKit
