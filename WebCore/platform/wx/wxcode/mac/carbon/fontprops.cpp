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
#include "fontprops.h"

#include <ApplicationServices/ApplicationServices.h>

#include <wx/defs.h>
#include <wx/gdicmn.h>

#ifdef BUILDING_ON_TIGER
void (*wkGetFontMetrics)(CGFontRef, int* ascent, int* descent, int* lineGap, unsigned* unitsPerEm);
#endif

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
#ifdef BUILDING_ON_TIGER
        wkGetFontMetrics(cgFont, &iAscent, &iDescent, &iLineGap, &unitsPerEm);
#else
        iAscent = CGFontGetAscent(cgFont);
        iDescent = CGFontGetDescent(cgFont);
        iLineGap = CGFontGetLeading(cgFont);
        unitsPerEm = CGFontGetUnitsPerEm(cgFont);
#endif
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

void GetTextExtent( const wxFont& font, const wxString& str, wxCoord *width, wxCoord *height,
                            wxCoord *descent, wxCoord *externalLeading )
{
    ATSUStyle* ATSUIStyle;

    if ( font.Ok() )
    {
        OSStatus status ;

        status = ATSUCreateAndCopyStyle( (ATSUStyle) font.MacGetATSUStyle() , (ATSUStyle*) &ATSUIStyle ) ;

        wxASSERT_MSG( status == noErr, wxT("couldn't create ATSU style") ) ;

        // we need the scale here ...

        Fixed atsuSize = IntToFixed( int( /*m_scaleY*/ 1 * font.GetPointSize()) ) ;
        //RGBColor atsuColor = MAC_WXCOLORREF( m_textForegroundColor.GetPixel() ) ;
        ATSUAttributeTag atsuTags[] =
        {
                kATSUSizeTag //,
        //        kATSUColorTag ,
        } ;
        ByteCount atsuSizes[sizeof(atsuTags) / sizeof(ATSUAttributeTag)] =
        {
                sizeof( Fixed ) //,
        //        sizeof( RGBColor ) ,
        } ;
        ATSUAttributeValuePtr atsuValues[sizeof(atsuTags) / sizeof(ATSUAttributeTag)] =
        {
                &atsuSize //,
        //        &atsuColor ,
        } ;

        status = ::ATSUSetAttributes(
            (ATSUStyle)ATSUIStyle, sizeof(atsuTags) / sizeof(ATSUAttributeTag) ,
            atsuTags, atsuSizes, atsuValues);

        wxASSERT_MSG( status == noErr , wxT("couldn't modify ATSU style") ) ;
    }
    
    wxCHECK_RET( ATSUIStyle != NULL, wxT("GetTextExtent - no valid font set") ) ;

    OSStatus status = noErr ;

    ATSUTextLayout atsuLayout ;
    UniCharCount chars = str.length() ;
    UniChar* ubuf = NULL ;

#if SIZEOF_WCHAR_T == 4
    wxMBConvUTF16 converter ;
#if wxUSE_UNICODE
    size_t unicharlen = converter.WC2MB( NULL , str.wc_str() , 0 ) ;
    ubuf = (UniChar*) malloc( unicharlen + 2 ) ;
    converter.WC2MB( (char*) ubuf , str.wc_str(), unicharlen + 2 ) ;
#else
    const wxWCharBuffer wchar = str.wc_str( wxConvLocal ) ;
    size_t unicharlen = converter.WC2MB( NULL , wchar.data() , 0 ) ;
    ubuf = (UniChar*) malloc( unicharlen + 2 ) ;
    converter.WC2MB( (char*) ubuf , wchar.data() , unicharlen + 2 ) ;
#endif
    chars = unicharlen / 2 ;
#else
#if wxUSE_UNICODE
    ubuf = (UniChar*) str.wc_str() ;
#else
    wxWCharBuffer wchar = str.wc_str( wxConvLocal ) ;
    chars = wxWcslen( wchar.data() ) ;
    ubuf = (UniChar*) wchar.data() ;
#endif
#endif

    status = ::ATSUCreateTextLayoutWithTextPtr( (UniCharArrayPtr) ubuf , 0 , chars , chars , 1 ,
        &chars , (ATSUStyle*) &ATSUIStyle , &atsuLayout ) ;

    wxASSERT_MSG( status == noErr , wxT("couldn't create the layout of the text") );

    ATSUTextMeasurement textBefore, textAfter ;
    ATSUTextMeasurement textAscent, textDescent ;

    status = ::ATSUGetUnjustifiedBounds( atsuLayout, kATSUFromTextBeginning, kATSUToTextEnd,
        &textBefore , &textAfter, &textAscent , &textDescent );

    if ( height )
        *height = FixedToInt(textAscent + textDescent) ;
    if ( descent )
        *descent = FixedToInt(textDescent) ;
    if ( externalLeading )
        *externalLeading = 0 ;
    if ( width )
        *width = FixedToInt(textAfter - textBefore) ;

#if SIZEOF_WCHAR_T == 4
    free( ubuf ) ;
#endif

    ::ATSUDisposeTextLayout(atsuLayout);
    ::ATSUDisposeStyle((ATSUStyle)ATSUIStyle);
}
