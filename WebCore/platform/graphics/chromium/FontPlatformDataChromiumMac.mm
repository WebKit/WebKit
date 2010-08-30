/*
 * This file is part of the internal font implementation.
 *
 * Copyright (C) 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
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

// This file is a clone of platform/graphics/mac/FontPlatformDataMac.mm.
// Because we want to support loading fonts between processes in the face of
// font loading being blocked by the sandbox, we must maintain a fork.
// Please maintain this file by performing parallel changes to it.
//
// The only changes from FontPlatformDataMac should be:
// - The functions at the top of this file for loading and caching fonts
//   cross-process.
// - Changes to FontPlatformData::FontPlatformData(NSFont,bool,bool)
// - Changes to FontPlatformData::setFont()
// - Changes to FontPlatformData::~FontPlatformData()
// - Calls to refMemoryFont() in FontPlatformData::operator= and copy
// constructor.
//
// All changes are marked with "Start/End Chromium Change"
//
// For all other differences, if it was introduced in this file, then the
// maintainer forgot to include it in the list; otherwise it is an update that
// should have been applied to this file but was not.


// Start Chromium Change
#import "config.h"
#import "../graphics/cocoa/FontPlatformData.h"

#import "ChromiumBridge.h"
#import "PlatformString.h"
#import "WebCoreSystemInterface.h"
#import <AppKit/NSFont.h>
#import <wtf/HashMap.h>
#import <wtf/RefCounted.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>

namespace WebCore {

namespace {

class MemoryActivatedFont;
typedef HashMap<ATSFontRef, MemoryActivatedFont*> FontRefMemoryFontHash;
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

FontRefMemoryFontHash& fontCacheByActualFontRef()
{
    DEFINE_STATIC_LOCAL(FontRefMemoryFontHash, realFontRefCache, ());
    return realFontRefCache;
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

// MemoryActivatedFont encapsulates a font loaded from another process and
// activated from memory.
//
// Responsibilities:
// * Holder for the CGFontRef & ATSFontRef belonging to the activated font.
// * Responsible for unloading the font container when done.
//
// Memory Management:
// The class is reference counted, with each instance of FontPlatformData that
// uses this class holding a reference to it.
// Entries in 2 hashes are maintained internally to allow quick lookup
// of existing instances for reuse:
// - fontCacheBySrcFontContainerRef() - key is the ATSFontContainerRef
//   corresponding to the *original in-process NSFont* whose loading was blocked
//   by the sandbox.
//   This is needed to allow lookup of a pre-existing MemoryActivatedFont when
//   creating a new FontPlatformData object.
// - fontCacheByActualFontRef() - key is the ATSFontRef corresponding to the
//   *new in-memory font* that we got from the browser process.
//   This is needed so that a FontPlatformData object can refer back to the
//   MemoryActivatedFont it's using. Currently this is only needed to release
//   the font on FontPlatformData destruction.
//
// Assumptions:
// This code assumes that an ATSFontRef is a unique identifier tied to an
// activated font.  i.e. After we activate a font, its ATSFontRef doesn't
// change.
// It also assumes that the ATSFoncontainerRef for two in-memory NSFonts that
// correspond to the same on-disk font file are always the same and don't change
// with time.
//
// Flushing caches:
// When the system notifies us of a system font cache flush, all FontDataCache
// objects are destroyed.  This should in turn dereference all
// MemoryActivatedFonts and thus unload all in-memory fonts.
class MemoryActivatedFont : public RefCounted<MemoryActivatedFont> {
public:
    // srcFontRef - ATSFontRef belonging to the NSFont object that failed to
    // load in-process.
    // container - a font container corresponding to an identical font that
    // we loaded cross-process.
    MemoryActivatedFont(ATSFontContainerRef srcFontContainerRef, ATSFontContainerRef container)
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
        
        // Add ourselves to caches.
        fontCacheBySrcFontContainerRef().add(m_srcFontContainerRef, this);
        fontCacheByActualFontRef().add(m_atsFontRef, this);
    }
    
    // Get cached CGFontRef corresponding to the in-memory font.
    CGFontRef cgFont()
    {
        return m_cgFont.get();
    }
    
    // Get cached ATSFontRef corresponding to the in-memory font.
    ATSFontRef atsFontRef()
    {
        return m_atsFontRef;
    }
    
    // Destructor - Unload font container from memory and remove ourselves
    // from hashes.
    ~MemoryActivatedFont()
    {
        if (m_cgFont.get()) {  // Object construction succeeded.
            // First remove ourselves from the caches.
            ASSERT(fontCacheBySrcFontContainerRef().contains(m_srcFontContainerRef));
            ASSERT(fontCacheByActualFontRef().contains(m_atsFontRef));
            
            fontCacheBySrcFontContainerRef().remove(m_srcFontContainerRef);
            fontCacheByActualFontRef().remove(m_atsFontRef);
            
            // Make sure the CGFont is destroyed before its font container.
            m_cgFont.releaseRef();
        }
        
        if (m_fontContainer != kATSFontContainerRefUnspecified)
            ATSFontDeactivate(m_fontContainer, 0, kATSOptionFlagsDefault);
    }

private:
    ATSFontContainerRef m_fontContainer;
    WTF::RetainPtr<CGFontRef> m_cgFont;
    ATSFontRef m_atsFontRef;
    ATSFontContainerRef m_srcFontContainerRef;
};

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
// The caller is responsbile for calling derefMemoryFont() on the ATSFontRef
// of the returned font.
// On failure this function returns 0, in which case the caller doesn't need
// to perform any additional cleanup.
MemoryActivatedFont* loadFontFromBrowserProcess(NSFont* nsFont)
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
      
    MemoryActivatedFont* font = fontCacheBySrcFontContainerRef().get(srcFontContainerRef);
    if (!font) {
        font = new MemoryActivatedFont(srcFontContainerRef, container);
        if (!font->cgFont()) {
            delete font;
            return 0;
        }
    } else {
        font->ref();
    }

    return font;
}

// deref() the MemoryActivatedFont corresponding to the given ATSFontRef.  If no
// corresponding MemoryActivatedFont object exists, no action is performed.
void derefMemoryFont(ATSFontRef fontRef)
{
    if (fontRef == kATSFontRefUnspecified)
        return;
    MemoryActivatedFont* font = fontCacheByActualFontRef().get(fontRef);
    if (font)
        font->deref();
}

// ref() the MemoryActivatedFont corresponding to the given ATSFontRef.  If no
// corresponding MemoryActivatedFont object exists, no action is performed.
void refMemoryFont(ATSFontRef fontRef)
{
    if (fontRef == kATSFontRefUnspecified)
        return;
    MemoryActivatedFont* font = fontCacheByActualFontRef().get(fontRef);
    if (font)
        font->ref();
}

// Given an NSFont, try to load a representation of that font into the cgFont
// parameter.  If loading is blocked by the sandbox, the font may be loaded
// cross-process.
// If sandbox loading also fails, a fallback font is loaded.
//
// Considerations:
// * cgFont must be CFReleas()ed by the caller when done.
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
void loadFont(NSFont* nsFont, float fontSize, NSFont*& outNSFont, CGFontRef& cgFont, ATSUFontID& fontID)
{
    outNSFont = nsFont;
    cgFont = CTFontCopyGraphicsFont(toCTFontRef(outNSFont), 0);
    MemoryActivatedFont* memFont = 0;
    if (OutOfProcessFontLoadingEnabled() && outNSFont && cgFont && isLastResortFont(cgFont)) {
        // Release old CGFontRef since it points at the LastResort font which we don't want.
        CFRelease(cgFont);
        cgFont = 0;
        
        // Font loading was blocked by the Sandbox.
        memFont = loadFontFromBrowserProcess(outNSFont);
        if (memFont) {
            cgFont = memFont->cgFont();
            
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
    
    if (memFont) {
        fontID = memFont->atsFontRef();
    } else {
        fontID = CTFontGetPlatformFont(toCTFontRef(outNSFont), 0);
    }
}

} // namespace
// End Chromium Change

FontPlatformData::FontPlatformData(NSFont *nsFont, bool syntheticBold, bool syntheticOblique)
    : m_syntheticBold(syntheticBold)
    , m_syntheticOblique(syntheticOblique)
    , m_font(nsFont)
#if !defined(BUILDING_ON_TIGER) && !defined(BUILDING_ON_LEOPARD) && !defined(BUILDING_ON_SNOW_LEOPARD)
    , m_isColorBitmapFont(CTFontGetSymbolicTraits(toCTFontRef(nsFont)) & kCTFontColorGlyphsTrait)
#else
    , m_isColorBitmapFont(false)
#endif
{
// Start Chromium Change
    m_size = nsFont ? [nsFont pointSize] : 0.0f;
    CGFontRef cgFont = 0;
    NSFont* loadedFont = 0;
    loadFont(nsFont, m_size, loadedFont, cgFont, m_atsuFontID);
    m_font = loadedFont;
    if (m_font)
        CFRetain(m_font);
    m_cgFont.adoptCF(cgFont);
// End Chromium Change
}

FontPlatformData::FontPlatformData(const FontPlatformData& f)
{
    m_font = f.m_font && f.m_font != reinterpret_cast<NSFont *>(-1) ? const_cast<NSFont *>(static_cast<const NSFont *>(CFRetain(f.m_font))) : f.m_font;

    m_syntheticBold = f.m_syntheticBold;
    m_syntheticOblique = f.m_syntheticOblique;
    m_size = f.m_size;
    m_cgFont = f.m_cgFont;
    m_atsuFontID = f.m_atsuFontID;
    m_isColorBitmapFont = f.m_isColorBitmapFont;
    // Start Chromium Change
    refMemoryFont(m_atsuFontID);
    // End Chromium Change
}

FontPlatformData::~FontPlatformData()
{
    if (m_font && m_font != reinterpret_cast<NSFont *>(-1))
        CFRelease(m_font);

    // Start Chromium Change
    derefMemoryFont(m_atsuFontID);
    // End Chromium Change
}

const FontPlatformData& FontPlatformData::operator=(const FontPlatformData& f)
{
    m_syntheticBold = f.m_syntheticBold;
    m_syntheticOblique = f.m_syntheticOblique;
    m_size = f.m_size;
    m_cgFont = f.m_cgFont;
    m_atsuFontID = f.m_atsuFontID;
    if (m_font == f.m_font)
        return *this;
    if (f.m_font && f.m_font != reinterpret_cast<NSFont *>(-1))
        CFRetain(f.m_font);
    if (m_font && m_font != reinterpret_cast<NSFont *>(-1))
        CFRelease(m_font);
    m_font = f.m_font;
    m_isColorBitmapFont = f.m_isColorBitmapFont;
    // Start Chromium Change
    refMemoryFont(m_atsuFontID);
    // End Chromium Change
    return *this;
}

void FontPlatformData::setFont(NSFont *font)
{
    if (m_font == font)
        return;
    if (font)
        CFRetain(font);
    if (m_font && m_font != reinterpret_cast<NSFont *>(-1))
        CFRelease(m_font);
    m_font = font;
    m_size = font ? [font pointSize] : 0.0f;
    
    // Start Chromium Change
    CGFontRef cgFont = 0;
    NSFont* loadedFont = 0;
    loadFont(m_font, m_size, loadedFont, cgFont, m_atsuFontID);
    
    // If loadFont replaced m_font with a fallback font, then release the
    // previous font to counter the retain above. Then retain the new font.
    if (loadedFont != m_font) {
        CFRelease(m_font);
        m_font = loadedFont;
        CFRetain(m_font);
    }
    m_cgFont.adoptCF(cgFont);
#if !defined(BUILDING_ON_TIGER) && !defined(BUILDING_ON_LEOPARD) && !defined(BUILDING_ON_SNOW_LEOPARD)
    m_isColorBitmapFont = CTFontGetSymbolicTraits(toCTFontRef(m_font)) & kCTFontColorGlyphsTrait;
#endif
    // End Chromium Change
}

bool FontPlatformData::roundsGlyphAdvances() const
{
    return [m_font renderingMode] == NSFontAntialiasedIntegerAdvancementsRenderingMode;
}

bool FontPlatformData::allowsLigatures() const
{
    return ![[m_font coveredCharacterSet] characterIsMember:'a'];
}

#if USE(CORE_TEXT)
CTFontRef FontPlatformData::ctFont() const
{
    if (m_font)
        return toCTFontRef(m_font);
    if (!m_CTFont)
        m_CTFont.adoptCF(CTFontCreateWithGraphicsFont(m_cgFont.get(), m_size, 0, 0));
    return m_CTFont.get();
}
#endif // USE(CORE_TEXT)

#ifndef NDEBUG
String FontPlatformData::description() const
{
    RetainPtr<CFStringRef> cgFontDescription(AdoptCF, CFCopyDescription(cgFont()));
    return String(cgFontDescription.get()) + " " + String::number(m_size) + (m_syntheticBold ? " synthetic bold" : "") + (m_syntheticOblique ? " synthetic oblique" : "");
}
#endif

} // namespace WebCore
