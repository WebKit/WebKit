/*	
    WebGlyphBuffer.h
    Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Cocoa/Cocoa.h>


@interface WebGlyphBuffer : NSObject
{
    NSFont *font;
    NSColor *color;
    float bufferedStartX, bufferedStartY;
    float lastPositionX, lastPositionY;
    int bufferedCount;
    int bufferSize;
    CGSize *bufferedAdvances;
    CGGlyph *bufferedGlyphs;
}
- (id)initWithFont:(NSFont *)font color:(NSColor *)color;
- (void)addGlyphs:(CGGlyph *)newGlyphs advances:(CGSize *)newAdvances count:(int)count at:(float)startX :(float)startY;
- (void)drawInView:(NSView *)targetView;
- (void)reset;
- (NSFont *)font;
- (NSColor *)color;
@end



