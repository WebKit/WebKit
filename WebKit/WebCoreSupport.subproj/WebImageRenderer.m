/*	IFImageRenderer.m
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/IFImageRenderer.h>
#import <WebKit/IFException.h>
#import <WebKit/WebKitDebug.h>

@implementation IFImageRenderer

static NSMutableArray *activeImageRenderers;

+ (void)stopAnimationsInView: (NSView *)aView
{
    int i, count;    

    count = [activeImageRenderers count];
    for (i = count-1; i >= 0; i--){
        IFImageRenderer *renderer = [activeImageRenderers objectAtIndex: i];
        if ([renderer frameView] == aView){
            [renderer stopAnimation];
        }
    }
    
}

- init
{
    lastStatus = -9999;
    lastLength = 0;
    return [super init];
}


- (BOOL)incrementalLoadWithBytes: (const void *)bytes length:(unsigned)length complete:(BOOL)isComplete
{
// FIXME:  This won't compile unless you have > 6C48.
#ifdef APPLE_PROGRESSIVE_IMAGE_LOADING

    NSBitmapImageRep* imageRep = [[self representations] objectAtIndex:0];
    //NSData *data = [[NSData alloc] initWithBytesNoCopy: (void *)bytes length: length freeWhenDone: NO];
    NSData *data = [[NSData alloc] initWithBytes: (void *)bytes length: length];
    int status;
    
    lastLength = length;
    lastStatus = status = [imageRep incrementalLoadFromData:data complete:isComplete];
    [data release];
    switch (status){
    case NSImageRepLoadStatusUnknownType:       // not enough data to determine image format. please feed me more data
        printf ("NSImageRepLoadStatusUnknownType size %d, isComplete %d\n", length, isComplete);
        return (lastReturn = NO);
    case NSImageRepLoadStatusReadingHeader:     // image format known, reading header. not yet valid. more data needed
        printf ("NSImageRepLoadStatusReadingHeader size %d, isComplete %d\n", length, isComplete);
        return (lastReturn = NO);
    case NSImageRepLoadStatusWillNeedAllData:   // can't read incrementally. will wait for complete data to become avail.
        printf ("NSImageRepLoadStatusWillNeedAllData size %d, isComplete %d\n", length, isComplete);
        return (lastReturn = NO);
    case NSImageRepLoadStatusInvalidData:       // image decompression encountered error.
        printf ("NSImageRepLoadStatusInvalidData size %d, isComplete %d\n", length, isComplete);
        return (lastReturn = NO);
    case NSImageRepLoadStatusUnexpectedEOF:     // ran out of data before full image was decompressed.
        printf ("NSImageRepLoadStatusUnexpectedEOF size %d, isComplete %d\n", length, isComplete);
        return (lastReturn = NO);
    case NSImageRepLoadStatusCompleted:         // all is well, the full pixelsHigh image is valid.
        printf ("NSImageRepLoadStatusUnexpectedEOF size %d, isComplete %d\n", length, isComplete);
        // Force the image to use the pixel size and ignore the dpi.
        [imageRep setSize:NSMakeSize([imageRep pixelsWide], [imageRep pixelsHigh])];
        return (lastReturn = YES);
    default:
        printf ("incrementalLoadFromData:complete: returned %d,  size %d, isComplete %d (wide %d, hight %d)\n", status, length, isComplete, [imageRep pixelsWide], [imageRep pixelsHigh]);
        return (lastReturn = YES);
    }
    return (lastReturn = NO);
#else
    [NSException raise: @"TEMPORARY!!  This code will be removed." format: @"foo"];
    return 0;
#endif
}

- (void)dealloc
{
    [self stopAnimation];
    [super dealloc];
}

- (id)firstRepProperty:(NSString *)propertyName
{
    id firstRep = [[self representations] objectAtIndex:0];
    id property = nil;
    if ([firstRep respondsToSelector:@selector(valueForProperty:)])
        property = [firstRep valueForProperty:propertyName];
    return property;
}

- (int)frameCount
{
    id property = [self firstRepProperty:NSImageFrameCount];
    return property != nil ? [property intValue] : 1;
}

- (int)currentFrame
{
    id property = [self firstRepProperty:NSImageCurrentFrame];
    return property != nil ? [property intValue] : 1;
}

- (void)setCurrentFrame:(int)frame
{
    NSBitmapImageRep* imageRep = [[self representations] objectAtIndex:0];
    [imageRep setProperty:NSImageCurrentFrame withValue:[NSNumber numberWithInt:frame]];
}

- (float)frameDuration
{
    id property = [self firstRepProperty:NSImageCurrentFrameDuration];
    float duration = (property != nil ? [property floatValue] : 0.0);
    if (duration < 0.0167){
        /*
            Many annoying ads specify a 0 duration to make an image flash
            as quickly as possible.  However a zero duration is faster than
            the refresh rate.  We need to pick a minimum duration.
            
            Browsers handle the minimum time case differently.  IE seems to use something
            close to 1/60th of a second.  Konqueror uses 0.  The ImageMagick library
            uses 1/100th.  The units in the GIF specification are 1/100th of second.
            We will use 1/60th of second as the minimum time.
        */
        duration = .0167;
    }
    return duration;
}

- (void)nextFrame:(id)context
{
    int currentFrame;
    
    // Release the timer that just fired.
    [frameTimer release];
    frameTimer = nil;
    
    currentFrame = [self currentFrame] + 1;
    if (currentFrame >= [self frameCount]) {
        currentFrame = 0;
    }
    [self setCurrentFrame:currentFrame];
    
    [frameView displayRect:targetRect];
    [[frameView window] flushWindow];

#if 0 // The following would be more efficient than using displayRect if we knew the image was opaque.
    if ([frameView canDraw]) {
        [frameView lockFocus];
        [self drawInRect:targetRect
                fromRect:imageRect
               operation:NSCompositeSourceOver	// Renders transparency correctly
                fraction:1.0];
        [frameView unlockFocus];
    }

    frameTimer = [[NSTimer scheduledTimerWithTimeInterval:[self frameDuration]
                                                   target:self
                                                 selector:@selector(nextFrame:)
                                                 userInfo:nil
                                                  repeats:NO] retain];
#endif
}

- (void)beginAnimationInView: (NSView *)view inRect: (NSRect)ir fromRect: (NSRect)fr
{
    // The previous, if any, frameView, is released in stopAnimation.
    [self stopAnimation];
    
    if ([self frameCount] > 1) {
        imageRect = fr;
        targetRect = ir;
        frameView = [view retain];
        frameTimer = [[NSTimer scheduledTimerWithTimeInterval:[self frameDuration]
                                                       target:self
                                                     selector:@selector(nextFrame:)
                                                     userInfo:nil
                                                      repeats:NO] retain];
        if (!activeImageRenderers)
            activeImageRenderers = [[NSMutableArray alloc] init];
            
        [activeImageRenderers addObject: self];
    }

    [self drawInRect: ir 
            fromRect: fr
           operation: NSCompositeSourceOver	// Renders transparency correctly
            fraction: 1.0];
}

- (NSView *)frameView
{
    return frameView;
}

- (void)stopAnimation
{
    [frameTimer invalidate];
    [frameTimer release];
    frameTimer = nil;
    
    [frameView release];
    frameView = nil;

    [activeImageRenderers removeObject: self];
}

- (void)resize:(NSSize)s
{
    [self setScalesWhenResized: YES];
    [self setSize: s];
}

// required by protocol -- apparently inherited methods don't count

- (NSSize)size
{
    return [super size];
}

- (void)drawInRect:(NSRect)dstRect fromRect:(NSRect)srcRect operation:(NSCompositingOperation)op fraction:(float)delta
{
    [super drawInRect:dstRect fromRect:srcRect operation:op fraction:delta];
}

@end
