/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "AppKitControlSystemImage.h"

#if USE(APPKIT)

#import "ColorMac.h"
#import "FloatRect.h"
#import "GraphicsContext.h"
#import "LocalDefaultSystemAppearance.h"
#import <pal/spi/mac/NSAppearanceSPI.h>

namespace WebCore {

AppKitControlSystemImage::AppKitControlSystemImage(AppKitControlSystemImageType controlType)
    : SystemImage(SystemImageType::AppKitControl)
    , m_controlType(controlType)
{
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    NSAppearance *appearance = [NSAppearance currentAppearance];
    ALLOW_DEPRECATED_DECLARATIONS_END
    m_tintColor = colorFromCocoaColor(appearance.tintColor);
    m_useDarkAppearance = [appearance.name isEqualToString:NSAppearanceNameDarkAqua];
}

void AppKitControlSystemImage::draw(GraphicsContext& graphicsContext, const FloatRect& rect) const
{
    LocalDefaultSystemAppearance localAppearance(m_useDarkAppearance, m_tintColor);
    drawControl(graphicsContext, rect);
}

} // namespace WebCore

#endif // USE(APPKIT)
