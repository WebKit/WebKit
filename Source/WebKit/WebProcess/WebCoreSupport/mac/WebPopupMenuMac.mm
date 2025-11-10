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

#import "config.h"
#import "WebPopupMenu.h"

#import "PlatformPopupMenuData.h"
#import <CoreText/CoreText.h>
#import <WebCore/LocalFrame.h>
#import <WebCore/LocalFrameView.h>
#import <WebCore/PopupMenuClient.h>

namespace WebKit {
using namespace WebCore;

void WebPopupMenu::setUpPlatformData(const IntRect&, PlatformPopupMenuData& data)
{
#if USE(APPKIT)
    CheckedPtr popupClient = m_popupClient;
    std::optional<InstalledFont> font = popupClient->menuStyle().checkedFont()->primaryFont()->toSerializableInstalledFont();
    if (!font) {
        double pointSize = popupClient->menuStyle().checkedFont()->primaryFont()->platformData().size();
        font = Font::create(FontPlatformData(bridge_cast([NSFont menuFontOfSize:pointSize]), pointSize))->toSerializableInstalledFont();
    }
    ASSERT(font);

    data.font = *font;
    data.shouldPopOver = popupClient->shouldPopOver();
    data.hideArrows = !popupClient->menuStyle().hasDefaultAppearance();
    data.menuSize = popupClient->menuStyle().menuSize();
#else
    UNUSED_PARAM(data);
#endif
}

} // namespace WebKit
