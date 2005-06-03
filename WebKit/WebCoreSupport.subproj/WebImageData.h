/*	WebImageData.h
	Copyright 2004, Apple, Inc. All rights reserved.
*/
#ifndef OMIT_TIGER_FEATURES

#import <Cocoa/Cocoa.h>

@class WebImageRenderer;

@interface WebImageData : NSObject <NSCopying>
{
    size_t imagesSize;
    CGImageRef *images;
    size_t imagePropertiesSize;
    CFDictionaryRef fileProperties;
    CFDictionaryRef *imageProperties;
    CGImageSourceRef imageSource;

    CGSize size;
    BOOL haveSize;
    
    CFMutableDictionaryRef animatingRenderers;
    NSTimer *frameTimer;

    size_t frameDurationsSize;
    float *frameDurations;
    
    size_t currentFrame;
    int repetitionsComplete;
    BOOL animationFinished;
    
    NSLock *decodeLock;
    
    id _PDFDoc;
    BOOL isPDF;
    
    BOOL isSolidColor;                              // Is frame 0 a solid color?
    CGColorRef solidColor;                          // If isSolidColor this is the color, or NULL for transparent
    
    BOOL sizeAvailable;
}

- (size_t)numberOfImages;
- (CGImageRef)imageAtIndex:(size_t)index;
- (BOOL)incrementalLoadWithBytes:(const void *)bytes length:(unsigned)length complete:(BOOL)isComplete callback:(id)c;
- (void)drawImageAtIndex:(size_t)index inRect:(CGRect)ir fromRect:(CGRect)fr compositeOperation:(NSCompositingOperation)op context:(CGContextRef)aContext;
- (void)drawImageAtIndex:(size_t)index inRect:(CGRect)ir fromRect:(CGRect)fr adjustedSize:(CGSize)size compositeOperation:(NSCompositingOperation)op context:(CGContextRef)aContext;
- (void)tileInRect:(CGRect)rect fromPoint:(CGPoint)point context:(CGContextRef)aContext;
- (BOOL)isNull;
- (CGSize)size;
- (void)animate;
- (BOOL)shouldAnimate;
+ (void)stopAnimationsInView:(NSView *)aView;
- (void)addAnimatingRenderer:(WebImageRenderer *)r inView:(NSView *)view;
- (void)removeAnimatingRenderer:(WebImageRenderer *)self;
- (BOOL)isAnimationFinished;
- (size_t)currentFrame;
- (CFDictionaryRef)propertiesAtIndex:(size_t)index;

- (void)decodeData:(CFDataRef)data isComplete:(BOOL)f callback:(id)c;

- (void)setIsPDF:(BOOL)f;
- (BOOL)isPDF;

- (void)resetAnimation;

@end

#endif
