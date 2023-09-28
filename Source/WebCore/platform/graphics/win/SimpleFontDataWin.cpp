/*
 * Copyright (C) 2006, 2007, 2008, 2013 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "Font.h"

#include "FontCache.h"
#include "FloatRect.h"
#include "FontDescription.h"
#include "HWndDC.h"
#include <mlang.h>
#include <wtf/MathExtras.h>
#include <wtf/win/GDIObject.h>

namespace WebCore {

void Font::platformCharWidthInit()
{
    m_avgCharWidth = 0;
    m_maxCharWidth = 0;
    initCharWidths();
}

void Font::platformDestroy()
{
    ScriptFreeCache(&m_scriptCache);
}

RefPtr<Font> Font::platformCreateScaledFont(const FontDescription&, float scaleFactor) const
{
    float scaledSize = scaleFactor * m_platformData.size();
    if (origin() == Origin::Remote)
        return Font::create(FontPlatformData::cloneWithSize(m_platformData, scaledSize), Font::Origin::Remote);

    LOGFONT winfont;
    GetObject(m_platformData.hfont(), sizeof(LOGFONT), &winfont);
    winfont.lfHeight = -lroundf(scaledSize * cWindowsFontScaleFactor);
    auto hfont = adoptGDIObject(::CreateFontIndirect(&winfont));
    return Font::create(FontPlatformData(WTFMove(hfont), scaledSize, m_platformData.syntheticBold(), m_platformData.syntheticOblique(), m_platformData.customPlatformData()), origin());
}

} // namespace WebCore
