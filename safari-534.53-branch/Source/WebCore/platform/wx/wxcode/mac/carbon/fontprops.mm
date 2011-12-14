/*
 * Copyright (C) 2007 Kevin Ollivier <kevino@theolliviers.com>
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
// this needs to be included before fontprops.h for UChar* to be defined.
#include <wtf/unicode/Unicode.h>

#include "fontprops.h"
#include "WebCoreSystemInterface.h"

#include <ApplicationServices/ApplicationServices.h>

#include <wx/defs.h>
#include <wx/gdicmn.h>
#include <wx/graphics.h>

const float smallCapsFontSizeMultiplier = 0.7f;
const float contextDPI = 72.0f;
static inline float scaleEmToUnits(float x, unsigned unitsPerEm) { return x * (contextDPI / (contextDPI * unitsPerEm)); }

wxFontProperties::wxFontProperties(wxFont* font):
m_ascent(0), m_descent(0), m_lineGap(0), m_lineSpacing(0), m_xHeight(0)
{
    CGFontRef cgFont;

#ifdef wxOSX_USE_CORE_TEXT && wxOSX_USE_CORE_TEXT
    cgFont = CTFontCopyGraphicsFont((CTFontRef)font->OSXGetCTFont(), NULL);
#else
    ATSFontRef fontRef;
    
    fontRef = FMGetATSFontRefFromFont(font->MacGetATSUFontID());
    
    if (fontRef)
        cgFont = CGFontCreateWithPlatformFont((void*)&fontRef);
#endif

    if (cgFont) {
        int iAscent;
        int iDescent;
        int iLineGap;
        unsigned unitsPerEm;
        iAscent = CGFontGetAscent(cgFont);
        iDescent = CGFontGetDescent(cgFont);
        iLineGap = CGFontGetLeading(cgFont);
        unitsPerEm = CGFontGetUnitsPerEm(cgFont);
        float pointSize = font->GetPointSize();
        float fAscent = scaleEmToUnits(iAscent, unitsPerEm) * pointSize;
        float fDescent = -scaleEmToUnits(iDescent, unitsPerEm) * pointSize;
        float fLineGap = scaleEmToUnits(iLineGap, unitsPerEm) * pointSize;

        m_ascent = lroundf(fAscent);
        m_descent = lroundf(fDescent);
        m_lineGap = lroundf(fLineGap);
        wxCoord xHeight = 0;
        GetTextExtent(*font, wxT("x"), NULL, &xHeight, NULL, NULL);
        m_xHeight = lroundf(xHeight);
        m_lineSpacing = m_ascent + m_descent + m_lineGap;

    }
    
    if (cgFont)
        CGFontRelease(cgFont);

}

bool wxFontContainsCharacters(void* font, const UChar* characters, int length)
{
    NSString* string = [[NSString alloc] initWithCharactersNoCopy:const_cast<unichar*>(characters) length:length freeWhenDone:NO];
    NSCharacterSet* set = [[(NSFont*)font coveredCharacterSet] invertedSet];
    bool result = set && [string rangeOfCharacterFromSet:set].location == NSNotFound;
    [string release];
    return result;
}

void GetTextExtent( const wxFont& font, const wxString& str, wxCoord *width, wxCoord *height,
                            wxCoord *descent, wxCoord *externalLeading )
{
    wxGraphicsContext * const gc = wxGraphicsContext::Create();
    
    gc->SetFont(font, *wxBLACK); // colour doesn't matter but must be specified
    struct GCTextExtent
    {
        wxDouble width, height, descent, externalLeading;
    } e;
    gc->GetTextExtent(str, &e.width, &e.height, &e.descent, &e.externalLeading);
    if ( width )
        *width = wxCoord(e.width + .5);
    if ( height )
        *height = wxCoord(e.height + .5);
    if ( descent )
        *descent = wxCoord(e.descent + .5);
    if ( externalLeading )
        *externalLeading = wxCoord(e.externalLeading + .5);

    delete gc;
}
