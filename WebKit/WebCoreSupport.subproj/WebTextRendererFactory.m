//
//  WebTextRendererFactory.m
//  WebKit
//
//  Created by Darin Adler on Thu May 02 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebKit/WebTextRendererFactory.h>
#import <WebKit/WebTextRenderer.h>
#import <WebKit/WebKitDebug.h>

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

+ (void)createSharedFactory;
{
    if (![self sharedFactory]) {
        [[[self alloc] init] release];
    }
    WEBKIT_ASSERT([[self sharedFactory] isMemberOfClass:self]);
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
            font = [[NSFontManager sharedFontManager] fontWithFamily:availableFamily traits:traits weight:5 size:size];
            if (font != nil) {
                return font;
            }
        }
    }
            
    WEBKITDEBUGLEVEL(WEBKIT_LOG_FONTCACHE, "unable to find font for family %s", [family lossyCString]);
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
