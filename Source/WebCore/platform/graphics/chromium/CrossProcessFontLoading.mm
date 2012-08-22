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

#import "../graphics/FontPlatformData.h"
#import "PlatformSupport.h"
#import <AppKit/NSFont.h>
#import <wtf/HashMap.h>

namespace WebCore {

namespace {

typedef HashMap<uint32, MemoryActivatedFont*> FontContainerRefMemoryFontHash;
typedef HashMap<WTF::String, MemoryActivatedFont*> FontNameMemoryFontHash;

// Caching:
//
// Requesting a font from the browser process is expensive and so is
// "activating" said font.  Caching of loaded fonts is complicated by the fact
// that it's impossible to get a unique identifier for the on-disk font file
// from inside the sandboxed renderer process.
// This means that when loading a font we need to round-trip through the browser
// process in order to get the unique font file identifer which we might already
// have activated and cached.
//
// In order to save as much work as we can, we maintain 2 levels of caching
// for the font data:
// 1. A dumb cache keyed by the font name/style (information we can determine
// from inside the sandbox).
// 2. A smarter cache keyed by the real "unique font id".
//
// In order to perform a lookup in #2 we need to consult with the browser to get
// us the lookup key.  While this doesn't save us the font load, it does save
// us font activation.
//
// It's important to remember that existing FontPlatformData objects are already
// cached, so a cache miss in the code in this file isn't necessarily so bad.

FontContainerRefMemoryFontHash& fontCacheByFontID()
{
    DEFINE_STATIC_LOCAL(FontContainerRefMemoryFontHash, srcFontIDCache, ());
    return srcFontIDCache;
}


FontNameMemoryFontHash& fontCacheByFontName()
{
    DEFINE_STATIC_LOCAL(FontNameMemoryFontHash, srcFontNameCache, ());
    return srcFontNameCache;
}

// Given a font specified by |srcFont|, use the information we can query in
// the sandbox to construct a key which we hope will be as unique as possible
// to the containing font file.
WTF::String hashKeyFromNSFont(NSFont* srcFont)
{
    NSFontDescriptor* desc = [srcFont fontDescriptor];
    NSFontSymbolicTraits traits = [desc symbolicTraits];
    return WTF::String::format("%s %x", [[srcFont fontName] UTF8String], traits);
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
    // First try to lookup in our cache with the limited information we have.
    WTF::String hashKey = hashKeyFromNSFont(nsFont);
    RefPtr<MemoryActivatedFont> font(fontCacheByFontName().get(hashKey));
    if (font)
        return font;

    CGFontRef tmpCGFont;
    uint32_t fontID;
    // Send cross-process request to load font.
    if (!PlatformSupport::loadFont(nsFont, &tmpCGFont, &fontID))
        return 0;

    RetainPtr<CGFontRef> cgFont(tmpCGFont);
    // Now that we have the fontID from the browser process, we can consult
    // the ID cache.
    font = fontCacheByFontID().get(fontID);
    if (font)
        // FIXME: PlatformSupport::loadFont() should consult the id cache
        // before activating the font.
        return font;

    return MemoryActivatedFont::create(fontID, nsFont, cgFont.get());
}

} // namespace

PassRefPtr<MemoryActivatedFont> MemoryActivatedFont::create(uint32_t fontID, NSFont* nsFont, CGFontRef cgFont)
{
  return adoptRef(new MemoryActivatedFont(fontID, nsFont, cgFont));
}

MemoryActivatedFont::MemoryActivatedFont(uint32_t fontID, NSFont* nsFont, CGFontRef cgFont)
    : m_cgFont(cgFont)
    , m_fontID(fontID)
    , m_inSandboxHashKey(hashKeyFromNSFont(nsFont))
{
    // Add ourselves to caches.
    fontCacheByFontID().add(fontID, this);
    fontCacheByFontName().add(m_inSandboxHashKey, this);
}

// Destructor - Unload font container from memory and remove ourselves
// from cache.
MemoryActivatedFont::~MemoryActivatedFont()
{
    // First remove ourselves from the caches.
    ASSERT(fontCacheByFontID().contains(m_fontID));
    ASSERT(fontCacheByFontName().contains(m_inSandboxHashKey));

    fontCacheByFontID().remove(m_fontID);
    fontCacheByFontName().remove(m_inSandboxHashKey);
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
// * outNSFont - The font that was actually loaded or null if loading failed.
// * cgFont - on output this contains the CGFontRef corresponding to the NSFont
//   that was picked in the end.  The caller is responsible for calling
//   CFRelease() on this parameter when done with it.
// * fontID - on output, the ID corresponding to nsFont.
void FontPlatformData::loadFont(NSFont* nsFont, float fontSize, NSFont*& outNSFont, CGFontRef& cgFont)
{
    outNSFont = nsFont;
    cgFont = CTFontCopyGraphicsFont(toCTFontRef(outNSFont), 0);
    if (outNSFont && cgFont && isLastResortFont(cgFont)) {
        // Release old CGFontRef since it points at the LastResort font which we don't want.
        CFRelease(cgFont);
        cgFont = 0;

        // Font loading was blocked by the Sandbox.
        m_inMemoryFont = loadFontFromBrowserProcess(outNSFont);
        if (m_inMemoryFont) {
            cgFont = m_inMemoryFont->cgFont();
            
            // Need to add an extra retain so output semantics of this function
            // are consistent.
            CFRetain(cgFont);
        } else {
            // If we still can't load the font, set |outNSFont| to null so that FontPlatformData won't be used.
            outNSFont = 0;
        }
    }
}

} // namespace WebCore
