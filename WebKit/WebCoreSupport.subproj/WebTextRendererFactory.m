/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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

#import <WebKit/WebAssertions.h>
#import <WebKit/WebBridge.h>
#import <WebKit/WebKitLogging.h>
#import <WebKit/WebKitSystemBits.h>
#import <WebKit/WebPreferences.h>
#import <WebKit/WebTextRendererFactory.h>
#import <WebKit/WebTextRenderer.h>
#import <WebKitSystemInterface.h>

#import <mach-o/dyld.h>

#define IMPORTANT_FONT_TRAITS (0 \
    | NSBoldFontMask \
    | NSCompressedFontMask \
    | NSCondensedFontMask \
    | NSExpandedFontMask \
    | NSItalicFontMask \
    | NSNarrowFontMask \
    | NSPosterFontMask \
    | NSSmallCapsFontMask \
)

#define DESIRED_WEIGHT 5

@interface NSFont (WebPrivate)
- (ATSUFontID)_atsFontID;
@end

@interface NSFont (WebAppKitSecretAPI)
- (BOOL)_isFakeFixedPitch;
@end

@implementation WebTextRendererFactory

- (BOOL)coalesceTextDrawing
{
    return [viewStack objectAtIndex: [viewStack count]-1] == [NSView focusView] ? YES : NO;
}

- (void)startCoalesceTextDrawing
{
    if (!viewStack)
        viewStack = [[NSMutableArray alloc] init];
    if (!viewBuffers)
        viewBuffers = [[NSMutableDictionary alloc] init];
    [viewStack addObject: [NSView focusView]];
}

- (void)endCoalesceTextDrawing
{
    ASSERT([self coalesceTextDrawing]);
    
    NSView *targetView = [viewStack objectAtIndex: [viewStack count]-1];
    [viewStack removeLastObject];
    NSValue *viewKey = [NSValue valueWithNonretainedObject: targetView];
    NSMutableSet *glyphBuffers = [viewBuffers objectForKey:viewKey];

    [glyphBuffers makeObjectsPerformSelector: @selector(drawInView:) withObject: targetView];
    [glyphBuffers makeObjectsPerformSelector: @selector(reset)];
    [viewBuffers removeObjectForKey: viewKey];
}

- (WebGlyphBuffer *)glyphBufferForFont: (NSFont *)font andColor: (NSColor *)color
{
    ASSERT([self coalesceTextDrawing]);

    NSMutableSet *glyphBuffers;
    WebGlyphBuffer *glyphBuffer = nil;
    NSValue *viewKey = [NSValue valueWithNonretainedObject: [NSView focusView]];
    
    glyphBuffers = [viewBuffers objectForKey:viewKey];
    if (glyphBuffers == nil){
        glyphBuffers = [[NSMutableSet alloc] init];
        [viewBuffers setObject: glyphBuffers forKey: viewKey];
        [glyphBuffers release];
    }
    
    NSEnumerator *enumerator = [glyphBuffers objectEnumerator];
    id value;
    
    // Could use a dictionary w/ font/color key for faster lookup.
    while ((value = [enumerator nextObject])) {
        if ([value font] == font && [[value color] isEqual: color])
            glyphBuffer = value;
    }
    if (glyphBuffer == nil){
        glyphBuffer = [[WebGlyphBuffer alloc] initWithFont: font color: color];
        [glyphBuffers addObject: glyphBuffer];
        [glyphBuffer release];
    }
        
    return glyphBuffer;
}

static bool
getAppDefaultValue(CFStringRef key, int *v)
{
    CFPropertyListRef value;

    value = CFPreferencesCopyValue(key, kCFPreferencesCurrentApplication,
                                   kCFPreferencesAnyUser,
                                   kCFPreferencesAnyHost);
    if (value == NULL) {
        value = CFPreferencesCopyValue(key, kCFPreferencesCurrentApplication,
                                       kCFPreferencesCurrentUser,
                                       kCFPreferencesAnyHost);
        if (value == NULL)
            return false;
    }

    if (CFGetTypeID(value) == CFNumberGetTypeID()) {
        if (v != NULL)
            CFNumberGetValue(value, kCFNumberIntType, v);
    } else if (CFGetTypeID(value) == CFStringGetTypeID()) {
        if (v != NULL)
            *v = CFStringGetIntValue(value);
    } else {
        CFRelease(value);
        return false;
    }

    CFRelease(value);
    return true;
}

static bool
getUserDefaultValue(CFStringRef key, int *v)
{
    CFPropertyListRef value;

    value = CFPreferencesCopyValue(key, kCFPreferencesAnyApplication,
                                   kCFPreferencesCurrentUser,
                                   kCFPreferencesCurrentHost);
    if (value == NULL)
        return false;

    if (CFGetTypeID(value) == CFNumberGetTypeID()) {
        if (v != NULL)
            CFNumberGetValue(value, kCFNumberIntType, v);
    } else if (CFGetTypeID(value) == CFStringGetTypeID()) {
        if (v != NULL)
            *v = CFStringGetIntValue(value);
    } else {
        CFRelease(value);
        return false;
    }

    CFRelease(value);
    return true;
}

static int getLCDScaleParameters(void)
{
    int mode;
    CFStringRef key;

    key = CFSTR("AppleFontSmoothing");
    if (!getAppDefaultValue(key, &mode)) {
        if (!getUserDefaultValue(key, &mode))
            return 1;
    }

	if (WKFontSmoothingModeIsLCD(mode))
		return 4;
	else
		return 1;
}

static CFMutableDictionaryRef fontCache;
static CFMutableDictionaryRef fixedPitchFonts;

- (void)clearCaches
{
    [cacheForScreen release];
    [cacheForPrinter release];
    
    cacheForScreen = [[NSMutableDictionary alloc] init];
    cacheForPrinter = [[NSMutableDictionary alloc] init];

    if (fontCache)
        CFRelease(fontCache);
    fontCache = NULL;
    
    if (fixedPitchFonts) {
        CFRelease (fixedPitchFonts);
        fixedPitchFonts = 0;
    }
        
    [super clearCaches];
}

static void 
fontsChanged( ATSFontNotificationInfoRef info, void *_factory)
{
    WebTextRendererFactory *factory = (WebTextRendererFactory *)_factory;
    
    LOG (FontCache, "clearing font caches");

    ASSERT (factory);

    [factory clearCaches];
}

#define MINIMUM_GLYPH_CACHE_SIZE 1536 * 1024

+ (void)createSharedFactory
{
    if (![self sharedFactory]) {
        [[[self alloc] init] release];

	size_t s;
	if (WebSystemMainMemory() > 128 * 1024 * 1024)
		s = MINIMUM_GLYPH_CACHE_SIZE*getLCDScaleParameters();
	else
		s = MINIMUM_GLYPH_CACHE_SIZE;
#ifndef NDEBUG
	LOG (CacheSizes, "Glyph cache size set to %d bytes.", s);
#endif
	WKSetUpFontCache(s);

        // Ignore errors returned from ATSFontNotificationSubscribe.  If we can't subscribe then we
        // won't be told about changes to fonts.
	ATSFontNotificationSubscribe( fontsChanged, kATSFontNotifyOptionDefault, (void *)[super sharedFactory], nil );
    }
    ASSERT([[self sharedFactory] isKindOfClass:self]);
}

+ (WebTextRendererFactory *)sharedFactory;
{
    return (WebTextRendererFactory *)[super sharedFactory];
}

- (BOOL)isFontFixedPitch: (NSFont *)font
{
    BOOL ret = NO;
    
    if (!fixedPitchFonts) {
        fixedPitchFonts = CFDictionaryCreateMutable (0, 0, &kCFTypeDictionaryKeyCallBacks, 0);
    }
    
    CFBooleanRef val = CFDictionaryGetValue (fixedPitchFonts, font);
    if (val) {
        ret = (val == kCFBooleanTrue);
    }
    else {
        // Special case Osaka-Mono.  According to <rdar://problem/3999467>, we should treat Osaka-Mono
        // as fixed pitch. Note that the AppKit does not report MS-PGothic as fixed pitch.

        // Special case MS PGothic.  According to <rdar://problem/4032938, we should not treat MS-PGothic
        // as fixed pitch.  Note that AppKit does report MS-PGothic as fixed pitch.
        if (([font isFixedPitch] || [font _isFakeFixedPitch] || [[font fontName] caseInsensitiveCompare:@"Osaka-Mono"] == NSOrderedSame) && 
                ![[font fontName] caseInsensitiveCompare:@"MS-PGothic"] == NSOrderedSame) {
            CFDictionarySetValue (fixedPitchFonts, font, kCFBooleanTrue);
            ret = YES;
        }
        else {
            CFDictionarySetValue (fixedPitchFonts, font, kCFBooleanFalse);
            ret = NO;
        }
    }
    return ret;    
}

- init
{
    [super init];
    
    cacheForScreen = [[NSMutableDictionary alloc] init];
    cacheForPrinter = [[NSMutableDictionary alloc] init];
    
    return self;
}

- (void)dealloc
{
    [cacheForScreen release];
    [cacheForPrinter release];
    [viewBuffers release];
    [viewStack release];
    
    [super dealloc];
}

- (WebTextRenderer *)rendererWithFont:(NSFont *)font usingPrinterFont:(BOOL)usingPrinterFont
{
    NSMutableDictionary *cache = usingPrinterFont ? cacheForPrinter : cacheForScreen;
    WebTextRenderer *renderer = [cache objectForKey:font];
    if (renderer == nil) {
        renderer = [[WebTextRenderer alloc] initWithFont:font usingPrinterFont:usingPrinterFont];
        [cache setObject:renderer forKey:font];
        [renderer release];
    }
    return renderer;
}

- (NSFont *)fallbackFontWithTraits:(NSFontTraitMask)traits size:(float)size 
{
    NSFont *font = [self cachedFontFromFamily:@"Helvetica" traits:traits size:size];
    if (font == nil) {
        // The Helvetica fallback will almost always work, since that's a basic
        // font that we ship with all systems. But in the highly unusual case where
        // the user removed Helvetica, we fall back on Lucida Grande because that's
        // guaranteed to be there, according to Nathan Taylor. This is good enough
        // to avoid a crash, at least. To reduce the possibility of failure even further,
        // we don't even bother with traits.
        font = [self cachedFontFromFamily:@"Lucida Grande" traits:0 size:size];
    }
    return font;
}

        

- (NSFont *)fontWithFamilies:(NSString **)families traits:(NSFontTraitMask)traits size:(float)size
{
    NSFont *font = nil;
    NSString *family;
    int i = 0;
    
    while (families && families[i] != 0 && font == nil){
        family = families[i++];
        if ([family length] != 0)
            font = [self cachedFontFromFamily: family traits:traits size:size];
    }
    if (font == nil) {
        // We didn't find a font.  Use a fallback font.
        static int matchCount = 3;
        static NSString *matchWords[] = { @"Arabic", @"Pashto", @"Urdu" };
        static NSString *matchFamilies[] = { @"Geeza Pro", @"Geeza Pro", @"Geeza Pro" };
        
        // First we'll attempt to find an appropriate font using a match based on 
        // the presence of keywords in the the requested names.  For example, we'll
        // match any name that contains "Arabic" to Geeza Pro.
        int j;
        i = 0;
        while (families && families[i] != 0 && font == nil) {
            family = families[i++];
            if ([family length] != 0) {
                j = 0;
                while (j < matchCount && font == nil) {
                    if ([family rangeOfString:matchWords[j] options:NSCaseInsensitiveSearch].location != NSNotFound) {
                        font = [self cachedFontFromFamily:matchFamilies[j] traits:traits size:size];
                    }
                    j++;
                }
            }
        }
        
        // Still nothing found, use the final fallback.
        if (font == nil) {
            font = [self fallbackFontWithTraits:traits size:size];
        }
    }

    return font;
}

static BOOL acceptableChoice(NSFontTraitMask desiredTraits, int desiredWeight,
    NSFontTraitMask candidateTraits, int candidateWeight)
{
    return (candidateTraits & desiredTraits) == desiredTraits;
}

static BOOL betterChoice(NSFontTraitMask desiredTraits, int desiredWeight,
    NSFontTraitMask chosenTraits, int chosenWeight,
    NSFontTraitMask candidateTraits, int candidateWeight)
{
    if (!acceptableChoice(desiredTraits, desiredWeight, candidateTraits, candidateWeight)) {
        return NO;
    }
    
    // A list of the traits we care about.
    // The top item in the list is the worst trait to mismatch; if a font has this
    // and we didn't ask for it, we'd prefer any other font in the family.
    const NSFontTraitMask masks[] = {
        NSPosterFontMask,
        NSSmallCapsFontMask,
        NSItalicFontMask,
        NSCompressedFontMask,
        NSCondensedFontMask,
        NSExpandedFontMask,
        NSNarrowFontMask,
        NSBoldFontMask,
        0 };
    int i = 0;
    NSFontTraitMask mask;
    while ((mask = masks[i++])) {
        if ((desiredTraits & mask) != 0) {
            ASSERT((chosenTraits & mask) != 0);
            ASSERT((candidateTraits & mask) != 0);
            continue;
        }
        BOOL chosenHasUnwantedTrait = (chosenTraits & mask) != 0;
        BOOL candidateHasUnwantedTrait = (candidateTraits & mask) != 0;
        if (!candidateHasUnwantedTrait && chosenHasUnwantedTrait) {
            return YES;
        }
        if (!chosenHasUnwantedTrait && candidateHasUnwantedTrait) {
            return NO;
        }
    }
    
    int chosenWeightDelta = chosenWeight - desiredWeight;
    int candidateWeightDelta = candidateWeight - desiredWeight;
    
    int chosenWeightDeltaMagnitude = ABS(chosenWeightDelta);
    int candidateWeightDeltaMagnitude = ABS(candidateWeightDelta);
    
    // Smaller magnitude wins.
    // If both have same magnitude, tie breaker is that the smaller weight wins.
    // Otherwise, first font in the array wins (should almost never happen).
    if (candidateWeightDeltaMagnitude < chosenWeightDeltaMagnitude) {
        return YES;
    }
    if (candidateWeightDeltaMagnitude == chosenWeightDeltaMagnitude && candidateWeight < chosenWeight) {
        return YES;
    }
    
    return NO;
}

// Family name is somewhat of a misnomer here.  We first attempt to find an exact match
// comparing the desiredFamily to the PostScript name of the installed fonts.  If that fails
// we then do a search based on the family names of the installed fonts.
- (NSFont *)fontWithFamily:(NSString *)desiredFamily traits:(NSFontTraitMask)desiredTraits size:(float)size
{
    NSFontManager *fontManager = [NSFontManager sharedFontManager];
    NSFont *font= nil;
    
    LOG (FontSelection, "looking for %@ with traits %x\n", desiredFamily, desiredTraits);
    
    // Look for an exact match first.
    NSEnumerator *availableFonts = [[fontManager availableFonts] objectEnumerator];
    NSString *availableFont;
    while ((availableFont = [availableFonts nextObject])) {
        if ([desiredFamily caseInsensitiveCompare:availableFont] == NSOrderedSame) {
            NSFont *nameMatchedFont = [NSFont fontWithName:availableFont size:size];
	    
	    // Special case Osaka-Mono.  According to <rdar://problem/3999467>, we need to 
	    // treat Osaka-Mono as fixed pitch.
	    if ([desiredFamily caseInsensitiveCompare:@"Osaka-Mono"] == NSOrderedSame && desiredTraits == 0) {
		LOG (FontSelection, "found exact match for Osaka-Mono\n");
		return nameMatchedFont;
	    }
	    
            NSFontTraitMask traits = [fontManager traitsOfFont:nameMatchedFont];
            
            if ((traits & desiredTraits) == desiredTraits){
                font = [fontManager convertFont:nameMatchedFont toHaveTrait:desiredTraits];
                LOG (FontSelection, "returning exact match (%@)\n\n", [[[font fontDescriptor] fontAttributes] objectForKey: NSFontNameAttribute]);
                return font;
            }
            LOG (FontSelection, "found exact match, but not desired traits, available traits %x\n", traits);
            break;
        }
    }
    
    // Do a simple case insensitive search for a matching font family.
    // NSFontManager requires exact name matches.
    // This addresses the problem of matching arial to Arial, etc., but perhaps not all the issues.
    NSEnumerator *e = [[fontManager availableFontFamilies] objectEnumerator];
    NSString *availableFamily;
    while ((availableFamily = [e nextObject])) {
        if ([desiredFamily caseInsensitiveCompare:availableFamily] == NSOrderedSame) {
            break;
        }
    }
    
    // Found a family, now figure out what weight and traits to use.
    BOOL choseFont = false;
    int chosenWeight = 0;
    NSFontTraitMask chosenTraits = 0;

    NSArray *fonts = [fontManager availableMembersOfFontFamily:availableFamily];    
    unsigned n = [fonts count];
    unsigned i;
    for (i = 0; i < n; i++) {
        NSArray *fontInfo = [fonts objectAtIndex:i];
        
        // Array indices must be hard coded because of lame AppKit API.
        int fontWeight = [[fontInfo objectAtIndex:2] intValue];
        NSFontTraitMask fontTraits = [[fontInfo objectAtIndex:3] unsignedIntValue];
        
        BOOL newWinner;
        
        if (!choseFont) {
            newWinner = acceptableChoice(desiredTraits, DESIRED_WEIGHT, fontTraits, fontWeight);
        } else {
            newWinner = betterChoice(desiredTraits, DESIRED_WEIGHT,
                chosenTraits, chosenWeight, fontTraits, fontWeight);
        }

        if (newWinner) {
            choseFont = YES;
            chosenWeight = fontWeight;
            chosenTraits = fontTraits;
            
            if (chosenWeight == DESIRED_WEIGHT
                    && (chosenTraits & IMPORTANT_FONT_TRAITS) == (desiredTraits & IMPORTANT_FONT_TRAITS)) {
                break;
            }
        }
    }
    
    if (!choseFont) {
        LOG (FontSelection, "nothing appropriate to return\n\n");
        return nil;
    }

    font = [fontManager fontWithFamily:availableFamily traits:chosenTraits weight:chosenWeight size:size];
    LOG (FontSelection, "returning font family %@ (%@) traits %x, fontID = %x\n\n", 
            availableFamily, [[[font fontDescriptor] fontAttributes] objectForKey: NSFontNameAttribute], chosenTraits, (unsigned int)[font _atsFontID]);
    
    return font;
}

typedef struct {
    NSString *family;
    NSFontTraitMask traits;
    float size;
} FontCacheKey;

static const void *FontCacheKeyCopy(CFAllocatorRef allocator, const void *value)
{
    const FontCacheKey *key = (const FontCacheKey *)value;
    FontCacheKey *result = malloc(sizeof(FontCacheKey));
    result->family = [key->family copy];
    result->traits = key->traits;
    result->size = key->size;
    return result;
}

static void FontCacheKeyFree(CFAllocatorRef allocator, const void *value)
{
    const FontCacheKey *key = (const FontCacheKey *)value;
    [key->family release];
    free((void *)key);
}

static Boolean FontCacheKeyEqual(const void *value1, const void *value2)
{
    const FontCacheKey *key1 = (const FontCacheKey *)value1;
    const FontCacheKey *key2 = (const FontCacheKey *)value2;
    return key1->size == key2->size && key1->traits == key2->traits && [key1->family isEqualToString:key2->family];
}

static CFHashCode FontCacheKeyHash(const void *value)
{
    const FontCacheKey *key = (const FontCacheKey *)value;
    return [key->family hash] ^ key->traits ^ (int)key->size;
}

static const void *FontCacheValueRetain(CFAllocatorRef allocator, const void *value)
{
    if (value != NULL) {
        CFRetain(value);
    }
    return value;
}

static void FontCacheValueRelease(CFAllocatorRef allocator, const void *value)
{
    if (value != NULL) {
        CFRelease(value);
    }
}

- (NSFont *)cachedFontFromFamily:(NSString *)family traits:(NSFontTraitMask)traits size:(float)size
{
    ASSERT(family);
    
    if (!fontCache) {
        static const CFDictionaryKeyCallBacks fontCacheKeyCallBacks = { 0, FontCacheKeyCopy, FontCacheKeyFree, NULL, FontCacheKeyEqual, FontCacheKeyHash };
        static const CFDictionaryValueCallBacks fontCacheValueCallBacks = { 0, FontCacheValueRetain, FontCacheValueRelease, NULL, NULL };
        fontCache = CFDictionaryCreateMutable(NULL, 0, &fontCacheKeyCallBacks, &fontCacheValueCallBacks);
    }

    const FontCacheKey fontKey = { family, traits, size };
    const void *value;
    NSFont *font;
    if (CFDictionaryGetValueIfPresent(fontCache, &fontKey, &value)) {
        font = (NSFont *)value;
    } else {
        if ([family length] == 0) {
            return nil;
        }
        font = [self fontWithFamily:family traits:traits size:size];
        CFDictionaryAddValue(fontCache, &fontKey, font);
    }
    
#ifdef DEBUG_MISSING_FONT
    static int unableToFindFontCount = 0;
    if (font == nil) {
        unableToFindFontCount++;
        NSLog(@"unableToFindFontCount %@, traits 0x%08x, size %f, %d\n", family, traits, size, unableToFindFontCount);
    }
#endif
    
    return font;
}

@end
