/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebFontRendering.h"

#if OS(LINUX)
#include "WebFontInfo.h"
#endif

namespace {

SkPaint::Hinting skiaHinting = SkPaint::kNormal_Hinting;
bool useSkiaAutoHint = true;
bool useSkiaBitmaps = true;
bool useSkiaAntiAlias = true;
bool useSkiaSubpixelRendering = false;
bool useSkiaSubpixelPositioning = false;

}

namespace WebKit {

// static
void WebFontRendering::setHinting(SkPaint::Hinting hinting)
{
    skiaHinting = hinting;
}

// static
SkPaint::Hinting WebFontRendering::hinting()
{
    return skiaHinting;
}

// static
void WebFontRendering::setAutoHint(bool useAutoHint)
{
    useSkiaAutoHint = useAutoHint;
}

// static
bool WebFontRendering::autoHint()
{
    return useSkiaAutoHint;
}

// static
void WebFontRendering::setUseBitmaps(bool useBitmaps)
{
    useSkiaBitmaps = useBitmaps;
}

// static
bool WebFontRendering::useBitmaps()
{
    return useSkiaBitmaps;
}

// static
void WebFontRendering::setAntiAlias(bool useAntiAlias)
{
    useSkiaAntiAlias = useAntiAlias;
}

// static
bool WebFontRendering::antiAlias()
{
    return useSkiaAntiAlias;
}

// static
void WebFontRendering::setSubpixelRendering(bool useSubpixelRendering)
{
    useSkiaSubpixelRendering = useSubpixelRendering;
}

// static
bool WebFontRendering::subpixelRendering()
{
    return useSkiaSubpixelRendering;
}

// static
void WebFontRendering::setSubpixelPositioning(bool useSubpixelPositioning)
{
    useSkiaSubpixelPositioning = useSubpixelPositioning;
#if OS(LINUX)
    WebFontInfo::setSubpixelPositioning(useSubpixelPositioning);
#endif
}

// static
bool WebFontRendering::subpixelPositioning()
{
    return useSkiaSubpixelPositioning;
}

// static
void WebFontRendering::setLCDOrder(SkFontHost::LCDOrder order)
{
    SkFontHost::SetSubpixelOrder(order);
}

// static
void WebFontRendering::setLCDOrientation(SkFontHost::LCDOrientation orientation)
{
    SkFontHost::SetSubpixelOrientation(orientation);
}

} // namespace WebKit
