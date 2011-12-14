/*
 * Copyright (C) 2010 Kevin Ollivier, Stefan Csomor  All rights reserved.
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
#include "FontPlatformData.h"

#include <wx/defs.h>
#include <wx/font.h>
#include <wx/fontutil.h>

#if !wxCHECK_VERSION(2,9,0)
#include <wx/mac/private.h>
#else
#include <wx/osx/private.h>
#endif

#if !wxCHECK_VERSION(2,9,0) || !wxOSX_USE_COCOA

static inline double DegToRad(double deg)
{
    return (deg * M_PI) / 180.0;
}

static const NSAffineTransformStruct kSlantNSTransformStruct = { 1, 0, tan(DegToRad(11)), 1, 0, 0  };

NSFont* OSXCreateNSFont(const wxNativeFontInfo* info)
{
    NSFont* nsFont;
    int weight = 5;
    NSFontTraitMask traits = 0;
    if (info->GetWeight() == wxFONTWEIGHT_BOLD)
    {
        traits |= NSBoldFontMask;
        weight = 9;
    }
    else if (info->GetWeight() == wxFONTWEIGHT_LIGHT)
        weight = 3;

    if (info->GetStyle() == wxFONTSTYLE_ITALIC || info->GetStyle() == wxFONTSTYLE_SLANT)
        traits |= NSItalicFontMask;
    
    nsFont = [[NSFontManager sharedFontManager] fontWithFamily:(NSString*)(CFStringRef)wxMacCFStringHolder(info->GetFaceName()) 
        traits:traits weight:weight size:info->GetPointSize()];
    
    if ( nsFont == nil )
    {
        NSFontTraitMask remainingTraits = traits;
        nsFont = [[NSFontManager sharedFontManager] fontWithFamily:(NSString*)(CFStringRef)wxMacCFStringHolder(info->GetFaceName()) 
                                                            traits:0 weight:5 size:info->GetPointSize()];
        if ( nsFont == nil )
        {
            if ( info->GetWeight() == wxFONTWEIGHT_BOLD )
            {
                nsFont = [NSFont boldSystemFontOfSize:info->GetPointSize()];
                remainingTraits &= ~NSBoldFontMask;
            }
            else
                nsFont = [NSFont systemFontOfSize:info->GetPointSize()];
        }
        
        // fallback - if in doubt, let go of the bold attribute
        if ( nsFont && (remainingTraits & NSItalicFontMask) )
        {
            NSFont* nsFontWithTraits = nil;
            if ( remainingTraits & NSBoldFontMask)
            {
                nsFontWithTraits = [[NSFontManager sharedFontManager] convertFont:nsFont toHaveTrait:NSBoldFontMask];
                if ( nsFontWithTraits == nil )
                {
                    nsFontWithTraits = [[NSFontManager sharedFontManager] convertFont:nsFont toHaveTrait:NSItalicFontMask];
                    if ( nsFontWithTraits != nil )
                        remainingTraits &= ~NSItalicFontMask;
                }
                else
                {
                    remainingTraits &= ~NSBoldFontMask;
                }
            }
            if ( remainingTraits & NSItalicFontMask)
            {
                if ( nsFontWithTraits == nil )
                    nsFontWithTraits = nsFont;
                
                NSAffineTransform* transform = [NSAffineTransform transform];
                [transform setTransformStruct:kSlantNSTransformStruct];
                [transform scaleBy:info->GetPointSize()];
                NSFontDescriptor* italicDesc = [[nsFontWithTraits fontDescriptor] fontDescriptorWithMatrix:transform];
                if ( italicDesc != nil )
                {
                    NSFont* f = [NSFont fontWithDescriptor:italicDesc size:(CGFloat)(info->GetPointSize())];
                    if ( f != nil )
                        nsFontWithTraits = f;
                }
            }
            if ( nsFontWithTraits != nil )
                nsFont = nsFontWithTraits;
        }
    }
            
    wxASSERT_MSG(nsFont != nil,wxT("Couldn't create nsFont")) ;
    wxMacCocoaRetain(nsFont);
    return nsFont;
}

#endif

namespace WebCore {

void FontPlatformData::cacheNSFont()
{
    if (m_nsFont)
        return;
        
#if wxCHECK_VERSION(2,9,1) && wxOSX_USE_COCOA
    if (m_font && m_font->font())
        m_nsFont = (NSFont*)m_font->font()->OSXGetNSFont();
#else
    m_nsFont = OSXCreateNSFont(m_font->font()->GetNativeFontInfo());
#endif
}

}
