/*	
    WebTextRendererFactory.m
    Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebAssertions.h>
#import <WebKit/WebKitLogging.h>
#import <WebKit/WebKitSystemBits.h>
#import <WebKit/WebTextRendererFactory.h>
#import <WebKit/WebTextRenderer.h>

#import <CoreGraphics/CoreGraphicsPrivate.h>
#import <CoreGraphics/CGFontLCDSupport.h>
#import <CoreGraphics/CGFontCache.h>

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

    switch (mode) {
        case kCGFontSmoothingLCDLight:
        case kCGFontSmoothingLCDMedium:
        case kCGFontSmoothingLCDStrong:
            return 4;
        default:
            return 1;
    }

}

#define MINIMUM_GLYPH_CACHE_SIZE 1536 * 1024

+ (void)createSharedFactory
{
    if (![self sharedFactory]) {
        [[[self alloc] init] release];

        // Turn off auto expiration of glyphs in CG's cache
        // and increase the cache size.
        NSSymbol symbol1 = NULL, symbol2 = NULL;
        if (NSIsSymbolNameDefined ("_CGFontCacheSetShouldAutoExpire")){
            symbol1 = NSLookupAndBindSymbol("_CGFontCacheSetShouldAutoExpire");
            symbol2 = NSLookupAndBindSymbol("_CGFontCacheSetMaxSize");
            if (symbol1 != NULL && symbol2 != NULL) {
                void (*functionPtr1)(CGFontCache *,bool) = NSAddressOfSymbol(symbol1);
                void (*functionPtr2)(CGFontCache *,size_t) = NSAddressOfSymbol(symbol2);
        
                CGFontCache *fontCache;
                fontCache = CGFontCacheCreate();
                functionPtr1 (fontCache, false);

                size_t s;
                if (WebSystemMainMemory() > 128 * 1024 * 1024)
                    s = MINIMUM_GLYPH_CACHE_SIZE*getLCDScaleParameters();
                else
                    s = MINIMUM_GLYPH_CACHE_SIZE;
#ifndef NDEBUG
                LOG (CacheSizes, "Glyph cache size set to %d bytes.", s);
#endif
                functionPtr2 (fontCache, s);
                CGFontCacheRelease(fontCache);
            }
        }

        if (symbol1 == NULL || symbol2 == NULL)
            NSLog(@"CoreGraphics is missing call to disable glyph auto expiration. Pages will load more slowly.");
    }
    ASSERT([[self sharedFactory] isKindOfClass:self]);
}

+ (WebTextRendererFactory *)sharedFactory;
{
    return (WebTextRendererFactory *)[super sharedFactory];
}

- (BOOL)isFontFixedPitch: (NSFont *)font
{
    return [font isFixedPitch] || [font _isFakeFixedPitch];
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
    return [self cachedFontFromFamily:@"Helvetica" traits:traits size:size];
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
    if (font == nil)
        font = [self fallbackFontWithTraits:traits size:size];

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

- (NSFont *)fontWithFamily:(NSString *)desiredFamily traits:(NSFontTraitMask)desiredTraits size:(float)size
{
    // Do a simple case insensitive search for a matching font family.
    // NSFontManager requires exact name matches.
    // This addresses the problem of matching arial to Arial, etc., but perhaps not all the issues.
    NSEnumerator *e = [[[NSFontManager sharedFontManager] availableFontFamilies] objectEnumerator];
    NSString *availableFamily;
    while ((availableFamily = [e nextObject])) {
        if ([desiredFamily caseInsensitiveCompare:availableFamily] == NSOrderedSame) {
            break;
        }
    }
    if (availableFamily == nil) {
        return nil;
    }
    
    // Found a family, now figure out what weight and traits to use.
    BOOL choseFont = false;
    int chosenWeight = 0;
    NSFontTraitMask chosenTraits = 0;

    NSArray *fonts = [[NSFontManager sharedFontManager] availableMembersOfFontFamily:availableFamily];    
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
        return nil;
    }
    
    return [[NSFontManager sharedFontManager] fontWithFamily:availableFamily traits:chosenTraits weight:chosenWeight size:size];
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
    
    static CFMutableDictionaryRef fontCache = NULL;
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
