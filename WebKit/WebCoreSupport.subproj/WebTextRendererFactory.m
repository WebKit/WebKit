/*	
    WebTextRendererFactory.m
    Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebTextRendererFactory.h>
#import <WebKit/WebTextRenderer.h>

#import <WebFoundation/WebAssertions.h>

#import <CoreGraphics/CoreGraphicsPrivate.h>

#import <mach-o/dyld.h>

@interface WebFontCacheKey : NSObject
{
    NSString *family;
    NSFontTraitMask traits;
    float size;
}

- initWithFamily:(NSString *)f traits:(NSFontTraitMask)t size:(float)s;

@end

@implementation WebFontCacheKey

- initWithFamily:(NSString *)f traits:(NSFontTraitMask)t size:(float)s;
{
    [super init];
    family = [f copy];
    traits = t;
    size = s;
    return self;
}

- (void)dealloc
{
    [family release];
    [super dealloc];
}

- (id)copyWithZone:(NSZone *)zone
{
    return [self retain];
}

- (unsigned)hash
{
    return [family hash] ^ traits ^ (int)size;
}

- (BOOL)isEqual:(id)o
{
    WebFontCacheKey *other = o;
    return [self class] == [other class]
        && [family isEqualToString:other->family]
        && traits == other->traits
        && size == other->size;
}

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

+ (void)createSharedFactory;
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
                functionPtr2 (fontCache, 1024*1024);
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

- init
{
    [super init];
    
    cache = [[NSMutableDictionary alloc] init];
    
    return self;
}

- (void)dealloc
{
    [cache release];
    [viewBuffers release];
    [viewStack release];
    
    [super dealloc];
}

- (WebTextRenderer *)rendererWithFont:(NSFont *)font
{
    WebTextRenderer *renderer = [cache objectForKey:font];
    if (renderer == nil) {
        renderer = [[WebTextRenderer alloc] initWithFont:font];
        [cache setObject:renderer forKey:font];
        [renderer release];
    }
    return renderer;
}

- (NSFont *)fontWithFamily:(NSString *)family traits:(NSFontTraitMask)traits size:(float)size
{
    NSFont *font;
    NSEnumerator *e;
    NSString *availableFamily;
    
    font = [[NSFontManager sharedFontManager] fontWithFamily:family traits:traits weight:5 size:size];
    if (font != nil) {
        return font;
    }
    
    // FIXME:  For now do a simple case insensitive search for a matching font.
    // The font manager requires exact name matches.  This will at least address the problem
    // of matching arial to Arial, etc.
    e = [[[NSFontManager sharedFontManager] availableFontFamilies] objectEnumerator];
    while ((availableFamily = [e nextObject])) {
        if ([family caseInsensitiveCompare:availableFamily] == NSOrderedSame) {
            NSArray *fonts = [[NSFontManager sharedFontManager] availableMembersOfFontFamily:availableFamily];
            NSArray *fontInfo;
            NSFontTraitMask fontMask;
            int fontWeight;
            unsigned i;
        
            for (i = 0; i < [fonts count]; i++){
                fontInfo = [fonts objectAtIndex: i];
                
                // Hard coded positions depend on lame AppKit API.
                fontWeight = [[fontInfo objectAtIndex: 2] intValue];
                fontMask = [[fontInfo objectAtIndex: 3] unsignedIntValue];
                
                // First look for a 'normal' weight font.  Note that the 
                // documentation indicates that the weight parameter is ignored if the 
                // trait contains the bold mask.  This is odd as one would think that other
                // traits could also indicate weight changes.  In fact, the weight parameter
                // and the trait mask together make a conflicted API.
                if (fontWeight == 5 && (fontMask & traits) == traits){
                    font = [[NSFontManager sharedFontManager] fontWithFamily:availableFamily traits:traits weight:5 size:size];
                    if (font != nil) {
                        return font;
                    }
                } 
                
                // Get a font with the correct traits but a weight we're told actually exists.
                if ((fontMask & traits) == traits){
                    font = [[NSFontManager sharedFontManager] fontWithFamily:availableFamily traits:traits weight:fontWeight size:size];
                    if (font != nil) {
                        return font;
                    }
                } 
            }
        }
    }
    
    return [[NSFontManager sharedFontManager] fontWithFamily:@"Helvetica" traits:traits weight:5 size:size];
}

- (NSFont *)cachedFontWithFamily:(NSString *)family traits:(NSFontTraitMask)traits size:(float)size
{
    static NSMutableDictionary *fontCache = nil;
    NSString *fontKey;
    NSFont *font;
    
#ifdef DEBUG_GETFONT
    static int getFontCount = 0;
    getFontCount++;
    printf("getFountCount = %d, family = %s, traits = 0x%08x, size = %f\n", getFontCount, [_family lossyCString], _trait, _size);
#endif

    if (!fontCache) {
        fontCache = [[NSMutableDictionary alloc] init];
    }
    
    fontKey = [[WebFontCacheKey alloc] initWithFamily:family traits:traits size:size];
    font = [fontCache objectForKey:fontKey];
    if (font == nil) {
        font = [self fontWithFamily:family traits:traits size:size];
        [fontCache setObject:font forKey:fontKey];
    }
    [fontKey release];
    
    return font;
}

- (id <WebCoreTextRenderer>)rendererWithFamily:(NSString *)family traits:(NSFontTraitMask)traits size:(float)size
{
    return [self rendererWithFont:[self cachedFontWithFamily:family traits:traits size:size]];
}

@end
