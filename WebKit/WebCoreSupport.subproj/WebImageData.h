/*	WebImageData.h
	Copyright 2004, Apple, Inc. All rights reserved.
*/
#if MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_3

#import <Cocoa/Cocoa.h>

// Needed for CGCompositeOperation
#import <CoreGraphics/CGContextPrivate.h>

@class WebImageRenderer;

@interface WebImageData : NSObject <NSCopying>
{
    CGImageRef *images;
    CGImageSourceRef imageSource;

    CFMutableDictionaryRef animatingRenderers;
    NSTimer *frameTimer;
    size_t currentFrame;
    int repetitionsComplete;
    BOOL animationFinished;
}

- (size_t)numberOfImages;
- (CGImageRef)imageAtIndex:(size_t)index;
- (BOOL)incrementalLoadWithBytes:(const void *)bytes length:(unsigned)length complete:(BOOL)isComplete;
- (void)drawImageAtIndex:(size_t)index inRect:(CGRect)ir fromRect:(CGRect)fr compositeOperation:(CGCompositeOperation)op context:(CGContextRef)aContext;
- (void)tileInRect:(CGRect)rect fromPoint:(CGPoint)point context:(CGContextRef)aContext;
- (BOOL)isNull;
- (CGSize)size;
- (void)animate;
+ (void)stopAnimationsInView:(NSView *)aView;
- (void)addAnimatingRenderer:(WebImageRenderer *)r inView:(NSView *)view;
- (void)removeAnimatingRenderer:(WebImageRenderer *)self;
- (BOOL)isAnimationFinished;
- (size_t)currentFrame;

@end

#endif
