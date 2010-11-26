/*
 * This file is part of the internal font implementation.
 *
 * Copyright (c) 2010 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

// This file provides additional functionality to the Mac FontPlatformData class
// defined in WebCore/platform/cocoa/FontPlatformDataCocoa.mm .
// Because we want to support loading fonts between processes in the face of
// font loading being blocked by the sandbox, we need a mechnasim to both
// do the loading of in-memory fonts and keep track of them.

#import "config.h"
#import "CrossProcessFontLoading.h"

#import "../graphics/cocoa/FontPlatformData.h"
#import "ChromiumBridge.h"
#import <AppKit/NSFont.h>
#import <wtf/HashMap.h>

namespace WebCore {

namespace {

typedef HashMap<ATSFontContainerRef, MemoryActivatedFont*> FontContainerRefMemoryFontHash;

// On 10.5, font loading is not blocked by the sandbox and thus there is no
// need for the cross-process font loading mechanim.
// On system versions >=10.6 cross-process font loading is required.
bool OutOfProcessFontLoadingEnabled()
{
    static SInt32 systemVersion = 0;
    if (!systemVersion) {
        if (Gestalt(gestaltSystemVersion, &systemVersion) != noErr)
            return false;
    }

    return systemVersion >= 0x1060;
}

FontContainerRefMemoryFontHash& fontCacheBySrcFontContainerRef()
{
    DEFINE_STATIC_LOCAL(FontContainerRefMemoryFontHash, srcFontRefCache, ());
    return srcFontRefCache;
}

ATSFontContainerRef fontContainerRefFromNSFont(NSFont* srcFont)
{
    ATSFontRef fontRef = CTFontGetPlatformFont(toCTFontRef(srcFont), 0);
    if (!fontRef)
        return kATSFontContainerRefUnspecified;
    ATSFontContainerRef fontContainer = kATSFontContainerRefUnspecified;
    if (ATSFontGetContainer(fontRef, 0, &fontContainer) != noErr)
        return kATSFontContainerRefUnspecified;
    return fontContainer;
}

// The only way we can tell that an in-process font has failed to load
// is if CTFontCopyGraphicsFont() returns the LastResort font.
bool isLastResortFont(CGFontRef cgFont)
{
    NSString* fontName = (NSString*)CGFontCopyPostScriptName(cgFont);
    return [fontName isEqualToString:@"LastResort"];
}

// Given an in-process font which has failed to load, return a
// MemoryActivatedFont* corresponding to an in-memory representation of the
// same font loaded from the browser process.
// On failure this function returns a PassRefPtr pointing to 0.
PassRefPtr<MemoryActivatedFont> loadFontFromBrowserProcess(NSFont* nsFont)
{
    ATSFontContainerRef container;
    // Send cross-process request to load font.
    if (!ChromiumBridge::loadFont(nsFont, &container))
        return 0;
    
    ATSFontContainerRef srcFontContainerRef = fontContainerRefFromNSFont(nsFont);
    if (!srcFontContainerRef) {
        ATSFontDeactivate(container, 0, kATSOptionFlagsDefault);
        return 0;
    }
    
    PassRefPtr<MemoryActivatedFont> font = adoptRef(fontCacheBySrcFontContainerRef().get(srcFontContainerRef));
    if (font.get())
        return font;

    return MemoryActivatedFont::create(srcFontContainerRef, container);
}

} // namespace

PassRefPtr<MemoryActivatedFont> MemoryActivatedFont::create(ATSFontContainerRef srcFontContainerRef, ATSFontContainerRef container)
{
  MemoryActivatedFont* font = new MemoryActivatedFont(srcFontContainerRef, container);
  if (!font->cgFont())  // Object construction failed.
  {
      delete font;
      return 0;
  }
  return adoptRef(font);
}

MemoryActivatedFont::MemoryActivatedFont(ATSFontContainerRef srcFontContainerRef, ATSFontContainerRef container)
    : m_fontContainer(container)
    , m_atsFontRef(kATSFontRefUnspecified)
    , m_srcFontContainerRef(srcFontContainerRef)
{
    if (!container)
        return;
    
    // Count the number of fonts in the container.
    ItemCount fontCount = 0;
    OSStatus err = ATSFontFindFromContainer(container, kATSOptionFlagsDefault, 0, 0, &fontCount);
    if (err != noErr || fontCount < 1)
        return;

    // For now always assume that we want the first font in the container.
    ATSFontFindFromContainer(container, kATSOptionFlagsDefault, 1, &m_atsFontRef, 0);

    if (!m_atsFontRef)
        return;

    // Cache CGFont representation of the font.
    m_cgFont.adoptCF(CGFontCreateWithPlatformFont(&m_atsFontRef));
    
    if (!m_cgFont.get())
        return;
    
    // Add ourselves to cache.
    fontCacheBySrcFontContainerRef().add(m_srcFontContainerRef, this);
}

// Destructor - Unload font container from memory and remove ourselves
// from cache.
MemoryActivatedFont::~MemoryActivatedFont()
{
    if (m_cgFont.get()) {
        // First remove ourselves from the caches.
        ASSERT(fontCacheBySrcFontContainerRef().contains(m_srcFontContainerRef));
        
        fontCacheBySrcFontContainerRef().remove(m_srcFontContainerRef);
        
        // Make sure the CGFont is destroyed before its font container.
        m_cgFont.releaseRef();
    }
    
    if (m_fontContainer != kATSFontContainerRefUnspecified)
        ATSFontDeactivate(m_fontContainer, 0, kATSOptionFlagsDefault);
}

// Given an NSFont, try to load a representation of that font into the cgFont
// parameter.  If loading is blocked by the sandbox, the font may be loaded
// cross-process.
// If sandbox loading also fails, a fallback font is loaded.
//
// Considerations:
// * cgFont must be CFRelease()ed by the caller when done.
// 
// Parameters:
// * nsFont - The font we wish to load.
// * fontSize - point size of the font we wish to load.
// * outNSFont - The font that was actually loaded, may be different from nsFont
//   if a fallback font was used.
// * cgFont - on output this contains the CGFontRef corresponding to the NSFont
//   that was picked in the end.  The caller is responsible for calling
//   CFRelease() on this parameter when done with it.
// * fontID - on output, the ID corresponding to nsFont.
void FontPlatformData::loadFont(NSFont* nsFont, float fontSize, NSFont*& outNSFont, CGFontRef& cgFont)
{
    outNSFont = nsFont;
    cgFont = CTFontCopyGraphicsFont(toCTFontRef(outNSFont), 0);
    if (OutOfProcessFontLoadingEnabled() && outNSFont && cgFont && isLastResortFont(cgFont)) {
        // Release old CGFontRef since it points at the LastResort font which we don't want.
        CFRelease(cgFont);
        cgFont = 0;
        
        // Font loading was blocked by the Sandbox.
        m_inMemoryFont = loadFontFromBrowserProcess(outNSFont);
        if (m_inMemoryFont.get()) {
            cgFont = m_inMemoryFont->cgFont();
            
            // Need to add an extra retain so output semantics of this function
            // are consistent.
            CFRetain(cgFont);
        } else {
            // If we still can't load the font, then return Times,
            // rather than the LastResort font.
            outNSFont = [NSFont fontWithName:@"Times" size:fontSize];
            cgFont = CTFontCopyGraphicsFont(toCTFontRef(outNSFont), 0);
        }
    }
}

} // namespace WebCore
