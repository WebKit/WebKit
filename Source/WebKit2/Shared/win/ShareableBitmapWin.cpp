/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "ShareableBitmap.h"

#include <WebCore/BitmapInfo.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/HWndDC.h>

using namespace WebCore;

namespace WebKit {

HDC ShareableBitmap::windowsContext() const
{
    ASSERT(isBackedBySharedMemory());
    if (m_windowsContext)
        return m_windowsContext.get();

    HWndDC screenDC(0);
    BitmapInfo bmInfo = BitmapInfo::createBottomUp(m_size);

    m_windowsContext = adoptPtr(::CreateCompatibleDC(screenDC));
    m_windowsBitmap = adoptPtr(CreateDIBSection(m_windowsContext.get(), &bmInfo, DIB_RGB_COLORS, 0, m_sharedMemory->handle(), 0));
    ::SelectObject(m_windowsContext.get(), m_windowsBitmap.get());

    return m_windowsContext.get();
}

} // namespace WebKit
