/*	IFAnimatedImage.m
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/IFImageRenderer.h>

#import <WebKit/WebKitDebug.h>

@implementation IFImageRenderer

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
    return property != nil ? [property floatValue] : 0.0;
}

- (void)nextFrame:(id)context
{
    int currentFrame = [self currentFrame];
    
    // Release the timer that just fired.
    [frameTimer release];
    
    currentFrame++;
    if (currentFrame >= [self frameCount]) {
        currentFrame = 0;
    }
    
    [self setCurrentFrame: currentFrame];
    
    if ([frameView canDraw]){
        [frameView lockFocus];
        [self drawInRect:targetRect
                fromRect:imageRect
               operation:NSCompositeSourceOver	// Renders transparency correctly
                fraction:1.0];
        [frameView unlockFocus];
        [[frameView window] flushWindow];
    }

    frameTimer = [[NSTimer scheduledTimerWithTimeInterval:[self frameDuration]
                                                   target:self
                                                 selector:@selector(nextFrame:)
                                                 userInfo:nil
                                                  repeats:NO] retain];
}

- (void)beginAnimationInView: (NSView *)view inRect: (NSRect)ir fromRect: (NSRect)fr
{
    WEBKIT_ASSERT(frameView == nil);
    
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
    }
}

- (void)stopAnimation
{
    [frameTimer invalidate];
    [frameTimer release];
    frameTimer = nil;
    
    [frameView release];
    frameView = nil;
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
