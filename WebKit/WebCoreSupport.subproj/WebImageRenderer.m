/*	IFAnimatedImage.m
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/IFImageRenderer.h>

@implementation IFImageRenderer

- (void)dealloc
{
    [self stopAnimation];
    [frameTimer release];
    [frameView release];
    [super dealloc];
}

- (int) frameCount {
    NSBitmapImageRep* imageRep = [[self representations] objectAtIndex:0];
    id property = nil;
    
    if ([imageRep respondsToSelector: @selector(valueForProperty:)])
        property = [imageRep valueForProperty:NSImageFrameCount];
    return property != nil ? [property intValue] : 1;
}

- (int) currentFrame {
    NSBitmapImageRep* imageRep = [[self representations] objectAtIndex:0];
    id property = nil;
    
    if ([imageRep respondsToSelector: @selector(valueForProperty:)])
        property = [imageRep valueForProperty:NSImageCurrentFrame];
    return property != nil ? [property intValue] : 1;
}

- (void) setCurrentFrame:(int)frame {
    NSBitmapImageRep* imageRep = [[self representations] objectAtIndex:0];
    [imageRep setProperty:NSImageCurrentFrame withValue: [NSNumber numberWithInt:frame]];
}

- (float) frameDuration {
    NSBitmapImageRep* imageRep = [[self representations] objectAtIndex:0];
    id property = nil;
    
    if ([imageRep respondsToSelector: @selector(valueForProperty:)])
        property = [imageRep valueForProperty:NSImageCurrentFrameDuration];
    return property != nil ? [property floatValue] : 0.0;
}

- (void)timerFired: (id)context
{
    int currentFrame = [self currentFrame];
    
    // Release the timer that just fired.
    [frameTimer release];
    
    currentFrame++;
    if (currentFrame >= [self frameCount]){
        currentFrame = 0;
    }
        
    [self setCurrentFrame: currentFrame];
    
    [frameView lockFocus];
    [self drawInRect: targetRect
            fromRect: imageRect
            operation: NSCompositeSourceOver	// Renders transparency correctly
            fraction: 1.0];
    [frameView unlockFocus];
    [[frameView window] flushWindow];

    float frameDuration = [self frameDuration];
    frameTimer = [[NSTimer scheduledTimerWithTimeInterval: frameDuration
                target: self
                selector: @selector(timerFired:)
                userInfo: nil
                repeats: NO] retain];
}

- (void)beginAnimationInView: (NSView *)view inRect: (NSRect)ir fromRect: (NSRect)fr
{
    [self stopAnimation];
    
    if ([self frameCount] > 1){
        float frameDuration = [self frameDuration];
        
        imageRect = fr;
        targetRect = ir;
        frameView = [view retain];
        frameTimer = [[NSTimer scheduledTimerWithTimeInterval: frameDuration
                    target: self
                    selector: @selector(timerFired:)
                    userInfo: nil
                    repeats: NO] retain];
    }
}

- (void)stopAnimation
{
    [frameTimer invalidate];
    [frameTimer release];
    frameTimer = 0;
    
    [frameView release];
    frameView = 0;
}

- (void)resize: (NSSize)s
{
    [self setScalesWhenResized: YES];
    [self setSize: s];
}

// require by protocol
- (NSSize)size 
{
    return [super size];
}

- (void)drawInRect:(NSRect)dstRect fromRect:(NSRect)srcRect operation:(NSCompositingOperation)op fraction:(float)delta
{
    [super drawInRect:dstRect fromRect:srcRect operation:op fraction:delta];
}


@end
