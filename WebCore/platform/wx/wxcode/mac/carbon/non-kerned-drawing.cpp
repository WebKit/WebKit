/*
 * Copyright (C) 2007 Kevin Watters, Kevin Ollivier.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FloatSize.h"
#include "GlyphBuffer.h"
#include "GraphicsContext.h"
#include "SimpleFontData.h"
#include <wtf/Vector.h>

#include <ApplicationServices/ApplicationServices.h>

#include <dlfcn.h>

#include <wx/defs.h>
#include <wx/dcclient.h>
#include <wx/dcgraph.h>
#include <wx/gdicmn.h>


// Unfortunately we need access to a private function to get the character -> glyph conversion needed to
// allow us to use CGContextShowGlyphsWithAdvances
// Note that on < 10.5, the function is called CGFontGetGlyphsForUnicodes, so we need to detect and deal
// with this.
typedef void (*CGFontGetGlyphsForUnicharsPtr)(CGFontRef, const UniChar[], const CGGlyph[], size_t);
static CGFontGetGlyphsForUnicharsPtr CGFontGetGlyphsForUnichars = (CGFontGetGlyphsForUnicharsPtr)dlsym(RTLD_DEFAULT, "CGFontGetGlyphsForUnichars");

namespace WebCore {

void drawTextWithSpacing(GraphicsContext* graphicsContext, const SimpleFontData* font, const wxColour& color, const GlyphBuffer& glyphBuffer, int from, int numGlyphs, const FloatPoint& point)
{
    graphicsContext->save();
    
    wxGCDC* dc = static_cast<wxGCDC*>(graphicsContext->platformContext());

    wxFont* wxfont = font->getWxFont();
    graphicsContext->setFillColor(graphicsContext->fillColor(), DeviceColorSpace);

    CGContextRef cgContext = static_cast<CGContextRef>(dc->GetGraphicsContext()->GetNativeContext());

    CGFontRef cgFont;

#ifdef wxOSX_USE_CORE_TEXT && wxOSX_USE_CORE_TEXT
    cgFont = CTFontCopyGraphicsFont((CTFontRef)wxfont->OSXGetCTFont(), NULL);
#else
    ATSFontRef fontRef;
    
    fontRef = FMGetATSFontRefFromFont(wxfont->MacGetATSUFontID());
    
    if (fontRef)
        cgFont = CGFontCreateWithPlatformFont((void*)&fontRef);
#endif
    
    CGContextSetFont(cgContext, cgFont);

    CGContextSetFontSize(cgContext, wxfont->GetPointSize());

    CGFloat red, green, blue, alpha;
    graphicsContext->fillColor().getRGBA(red, green, blue, alpha);
    CGContextSetRGBFillColor(cgContext, red, green, blue, alpha);

    CGAffineTransform matrix = CGAffineTransformIdentity;
    matrix.b = -matrix.b;
    matrix.d = -matrix.d;
    
    CGContextSetTextMatrix(cgContext, matrix);

    CGContextSetTextPosition(cgContext, point.x(), point.y());
    
    const FloatSize* advanceSizes = static_cast<const FloatSize*>(glyphBuffer.advances(from));
    int size = glyphBuffer.size() - from;
    CGSize sizes[size];
    CGGlyph glyphs[numGlyphs];
    
    // if the function doesn't exist, we're probably on tiger and need to grab the
    // function under its old name, CGFontGetGlyphsForUnicodes
    if (!CGFontGetGlyphsForUnichars)
        CGFontGetGlyphsForUnichars = (CGFontGetGlyphsForUnicharsPtr)dlsym(RTLD_DEFAULT, "CGFontGetGlyphsForUnicodes");
    
    // Let's make sure we got the function under one name or another!
    ASSERT(CGFontGetGlyphsForUnichars);
    CGFontGetGlyphsForUnichars(cgFont, glyphBuffer.glyphs(from), glyphs, numGlyphs);
    
    for (int i = 0; i < size; i++) {
        FloatSize fsize = advanceSizes[i];
        sizes[i] = CGSizeMake(fsize.width(), fsize.height());
    }
    
    CGContextShowGlyphsWithAdvances(cgContext, glyphs, sizes, numGlyphs);
    
    if (cgFont)
        CGFontRelease(cgFont);
    graphicsContext->restore();
}

}
