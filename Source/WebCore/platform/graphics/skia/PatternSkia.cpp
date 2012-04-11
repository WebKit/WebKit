/*
 * Copyright (C) 2008 Google Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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
#include "Pattern.h"

#include "AffineTransform.h"
#include "Image.h"
#include "NativeImageSkia.h"

#include "SkCanvas.h"
#include "SkColor.h"
#include "SkColorShader.h"
#include "SkShader.h"

#if USE(V8)
#include <v8.h>
#endif

using namespace std;

namespace WebCore {

void Pattern::platformDestroy()
{
    SkSafeUnref(m_pattern);
    m_pattern = 0;
    if (m_externalMemoryAllocated) {
#if USE(V8)
        v8::V8::AdjustAmountOfExternalAllocatedMemory(-m_externalMemoryAllocated);
#endif
        m_externalMemoryAllocated = 0;
    }
}

PlatformPatternPtr Pattern::platformPattern(const AffineTransform& patternTransform)
{
    if (m_pattern)
        return m_pattern;

    // Note: patternTransform is ignored since it seems to be applied elsewhere
    // (when the pattern is used?). Applying it to the pattern (i.e.
    // shader->setLocalMatrix) results in a double transformation. This can be
    // seen, for instance, as an extra offset in:
    // LayoutTests/fast/canvas/patternfill-repeat.html
    // and expanded scale and skew in:
    // LayoutTests/svg/W3C-SVG-1.1/pservers-grad-06-b.svg

    const NativeImageSkia* image = m_tileImage->nativeImageForCurrentFrame();
    // If we don't have a bitmap, return a transparent shader.
    if (!image)
        m_pattern = new SkColorShader(SkColorSetARGB(0, 0, 0, 0));

    else if (m_repeatX && m_repeatY)
        m_pattern = SkShader::CreateBitmapShader(image->bitmap(), SkShader::kRepeat_TileMode, SkShader::kRepeat_TileMode);

    else {

        // Skia does not have a "draw the tile only once" option. Clamp_TileMode
        // repeats the last line of the image after drawing one tile. To avoid
        // filling the space with arbitrary pixels, this workaround forces the
        // image to have a line of transparent pixels on the "repeated" edge(s),
        // thus causing extra space to be transparent filled.
        SkShader::TileMode tileModeX = m_repeatX ? SkShader::kRepeat_TileMode : SkShader::kClamp_TileMode;
        SkShader::TileMode tileModeY = m_repeatY ? SkShader::kRepeat_TileMode : SkShader::kClamp_TileMode;
        int expandW = m_repeatX ? 0 : 1;
        int expandH = m_repeatY ? 0 : 1;

        // Create a transparent bitmap 1 pixel wider and/or taller than the
        // original, then copy the orignal into it.
        // FIXME: Is there a better way to pad (not scale) an image in skia?
        SkBitmap bm2;
        bm2.setConfig(image->bitmap().config(), image->bitmap().width() + expandW, image->bitmap().height() + expandH);
        bm2.allocPixels();
        bm2.eraseARGB(0x00, 0x00, 0x00, 0x00);
        SkCanvas canvas(bm2);
        canvas.drawBitmap(image->bitmap(), 0, 0);
        m_pattern = SkShader::CreateBitmapShader(bm2, tileModeX, tileModeY);

        // Clamp to int, since that's what the adjust function takes.
        m_externalMemoryAllocated = static_cast<int>(min(static_cast<size_t>(INT_MAX), bm2.getSafeSize()));
#if USE(V8)
        v8::V8::AdjustAmountOfExternalAllocatedMemory(m_externalMemoryAllocated);
#endif
    }
    m_pattern->setLocalMatrix(m_patternSpaceTransformation);
    return m_pattern;
}

void Pattern::setPlatformPatternSpaceTransform()
{
    if (m_pattern)
        m_pattern->setLocalMatrix(m_patternSpaceTransformation);
}

} // namespace WebCore
