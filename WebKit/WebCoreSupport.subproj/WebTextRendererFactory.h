/*	
    WebTextRendererFactory.m
    Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebCore/WebCoreTextRendererFactory.h>
#import <WebKit/WebGlyphBuffer.h>

@class WebTextRenderer;

@interface WebTextRendererFactory : WebCoreTextRendererFactory
{
    NSMutableDictionary *cacheForScreen;
    NSMutableDictionary *cacheForPrinter;
    NSMutableDictionary *viewBuffers;
    NSMutableArray *viewStack;
}

+ (void)createSharedFactory;
+ (WebTextRendererFactory *)sharedFactory;
- (NSFont *)cachedFontFromFamily:(NSString *)family traits:(NSFontTraitMask)traits size:(float)size;

- (WebTextRenderer *)rendererWithFont:(NSFont *)font usingPrinterFont:(BOOL)usingPrinterFont;

- (BOOL)coalesceTextDrawing;
- (void)endCoalesceTextDrawing;
- (void)startCoalesceTextDrawing;

- (WebGlyphBuffer *)glyphBufferForFont:(NSFont *)font andColor:(NSColor *)color;

@end

@interface NSFont (WebPrivateExtensions)
- (BOOL)_web_isFakeFixedPitch;
@end

