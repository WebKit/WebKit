/*	
        WebImageRenderer.m
	Copyright (c) 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebImageRenderer.h>

#import <WebFoundation/WebAssertions.h>

#define MINIMUM_DURATION (1.0/30.0)

@implementation WebImageRenderer

static NSMutableArray *activeImageRenderers;

+ (void)stopAnimationsInView:(NSView *)aView
{
    int i, count;    

    count = [activeImageRenderers count];
    for (i = count-1; i >= 0; i--) {
        WebImageRenderer *renderer = [activeImageRenderers objectAtIndex:i];
        if (renderer->frameView == aView) {
            [renderer stopAnimation];
        }
    }
}

- copyWithZone:(NSZone *)zone
{
    WebImageRenderer *copy = [super copyWithZone:zone];
    
    // FIXME: If we copy while doing an incremental load, it won't work.
    
    copy->frameTimer = nil;
    copy->frameView = nil;
    copy->patternColor = nil;
    
    return copy;
}

- (BOOL)incrementalLoadWithBytes:(const void *)bytes length:(unsigned)length complete:(BOOL)isComplete
{
    NSBitmapImageRep *imageRep = [[self representations] objectAtIndex:0];
    NSData *data = [[NSData alloc] initWithBytes:bytes length:length];
    NSSize size;
    
    loadStatus = [imageRep incrementalLoadFromData:data complete:isComplete];
    [data release];
    switch (loadStatus) {
    case NSImageRepLoadStatusUnknownType:       // not enough data to determine image format. please feed me more data
        //printf ("NSImageRepLoadStatusUnknownType size %d, isComplete %d\n", length, isComplete);
        return NO;
    case NSImageRepLoadStatusReadingHeader:     // image format known, reading header. not yet valid. more data needed
        //printf ("NSImageRepLoadStatusReadingHeader size %d, isComplete %d\n", length, isComplete);
        return NO;
    case NSImageRepLoadStatusWillNeedAllData:   // can't read incrementally. will wait for complete data to become avail.
        //printf ("NSImageRepLoadStatusWillNeedAllData size %d, isComplete %d\n", length, isComplete);
        return NO;
    case NSImageRepLoadStatusInvalidData:       // image decompression encountered error.
        //printf ("NSImageRepLoadStatusInvalidData size %d, isComplete %d\n", length, isComplete);
        return NO;
    case NSImageRepLoadStatusUnexpectedEOF:     // ran out of data before full image was decompressed.
        //printf ("NSImageRepLoadStatusUnexpectedEOF size %d, isComplete %d\n", length, isComplete);
        return NO;
    case NSImageRepLoadStatusCompleted:         // all is well, the full pixelsHigh image is valid.
        //printf ("NSImageRepLoadStatusUnexpectedEOF size %d, isComplete %d\n", length, isComplete);
        // Force the image to use the pixel size and ignore the dpi.
        size = NSMakeSize([imageRep pixelsWide], [imageRep pixelsHigh]);
        [imageRep setSize:size];
        [self setSize:size];
        return YES;
    default:
        // We have some data.  Return YES so we can attempt to draw what we've got.
        return YES;
    }
    
    return NO;
}

- (void)dealloc
{
    [self stopAnimation];
    [patternColor release];
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
    return property ? [property intValue] : 1;
}

- (int)currentFrame
{
    id property = [self firstRepProperty:NSImageCurrentFrame];
    return property ? [property intValue] : 1;
}

- (void)setCurrentFrame:(int)frame
{
    NSBitmapImageRep *imageRep = [[self representations] objectAtIndex:0];
    [imageRep setProperty:NSImageCurrentFrame withValue:[NSNumber numberWithInt:frame]];
}

- (float)unadjustedFrameDuration
{
    id property = [self firstRepProperty:NSImageCurrentFrameDuration];
    float duration = property ? [property floatValue] : 0.0;
    return duration;
}

- (float)frameDuration
{
    float duration = [self unadjustedFrameDuration];
    if (duration < MINIMUM_DURATION) {
        /*
            Many annoying ads specify a 0 duration to make an image flash
            as quickly as possible.  However a zero duration is faster than
            the refresh rate.  We need to pick a minimum duration.
            
            Browsers handle the minimum time case differently.  IE seems to use something
            close to 1/30th of a second.  Konqueror uses 0.  The ImageMagick library
            uses 1/100th.  The units in the GIF specification are 1/100th of second.
            We will use 1/30th of second as the minimum time.
        */
        duration = MINIMUM_DURATION;
    }
    return duration;
}

- (void)scheduleFrame
{   
    frameTimer = [[NSTimer scheduledTimerWithTimeInterval:[self frameDuration]
                                                    target:self
                                                  selector:@selector(nextFrame:)
                                                  userInfo:nil
                                                   repeats:NO] retain];
}

- (void)drawClippedToValidInRect:(NSRect)ir fromRect:(NSRect)fr
{
    if (loadStatus > 0) {
        int pixelsHigh = [[[self representations] objectAtIndex:0] pixelsHigh];
        if (pixelsHigh > loadStatus) {
            // Figure out how much of the image is OK to draw.
            float clippedImageHeight = floor([self size].height * loadStatus / pixelsHigh);
            
            // Figure out how much of the source is OK to draw from.
            float clippedSourceHeight = clippedImageHeight - fr.origin.y;
            if (clippedSourceHeight < 1) {
                return;
            }
            
            // Figure out how much of the destination we are going to draw to.
            float clippedDestinationHeight = ir.size.height * clippedSourceHeight / fr.size.height;

            // Reduce heights of both rectangles without changing their positions.
            // In the non-flipped case, this means moving the origins up from the bottom left.
            // In the flipped case, just adjusting the height is sufficient.
            if (![[NSView focusView] isFlipped]) {
                ir.origin.y += ir.size.height - clippedDestinationHeight;
            }
            ir.size.height = clippedDestinationHeight;
            fr.origin.y += fr.size.height - clippedSourceHeight;
            fr.size.height = clippedSourceHeight;
        }
    }
    
    // This is the operation that handles transparent portions of the source image correctly.
    [self drawInRect:ir fromRect:fr operation:NSCompositeSourceOver fraction:1.0];
}

- (void)nextFrame:(id)context
{
    int currentFrame;
    NSWindow *window;
    
    // Release the timer that just fired.
    [frameTimer release];
    frameTimer = nil;
    
    currentFrame = [self currentFrame] + 1;
    if (currentFrame >= [self frameCount]) {
	// Don't repeat if the last frame has a duration of 0.  
        // IE doesn't repeat, so we don't.
        if ([self unadjustedFrameDuration] == 0) {
            animationFinished = YES;
            return;
        }
        currentFrame = 0;
    }
    [self setCurrentFrame:currentFrame];
    
    window = [frameView window];
    
     // We can't use isOpaque because it returns YES for many non-opaque images (Radar 2966937).
     // But we can at least assume that any image representation without alpha is opaque.
     if (![[[self representations] objectAtIndex:0] hasAlpha]) {
        if ([frameView canDraw]) {
            [frameView lockFocus];
            [self drawClippedToValidInRect:imageRect fromRect:targetRect];
            [frameView unlockFocus];
        }
        if (!animationFinished) {
            [self scheduleFrame];
        }
    } else {
        // No need to schedule the next frame in this case.  The display
        // will eventually cause the image to be redrawn and the next frame
        // will be scheduled in beginAnimationInRect:fromRect:.
        [frameView displayRect:targetRect];
    }
    
    [window flushWindow];
}

- (void)beginAnimationInRect:(NSRect)ir fromRect:(NSRect)fr
{
    // The previous, if any, frameView, is released in stopAnimation.
    [self stopAnimation];

    [self drawClippedToValidInRect:ir fromRect:fr];
    
    if ([self frameCount] > 1 && !animationFinished) {
        imageRect = fr;
        targetRect = ir;
        frameView = [[NSView focusView] retain];
        [self scheduleFrame];
	if (!activeImageRenderers) {
	    activeImageRenderers = [[NSMutableArray alloc] init];
	}
	[activeImageRenderers addObject:self];
    }
}

- (void)stopAnimation
{
    [frameTimer invalidate];
    [frameTimer release];
    frameTimer = nil;
    
    [frameView release];
    frameView = nil;

    [activeImageRenderers removeObject:self];
}

- (void)resize:(NSSize)s
{
    [self setScalesWhenResized:YES];
    [self setSize:s];
}

- (void)tileInRect:(NSRect)rect fromPoint:(NSPoint)point
{
    // These calculations are only correct for the flipped case.
    ASSERT([[NSView focusView] isFlipped]);

    NSSize size = [self size];

    // Check and see if a single draw of the image can cover the entire area we are supposed to tile.
    NSRect oneTileRect;
    oneTileRect.origin.x = rect.origin.x + fmodf(fmodf(-point.x, size.width) - size.width, size.width);
    oneTileRect.origin.y = rect.origin.y + fmodf(fmodf(-point.y, size.height) - size.height, size.height);
    oneTileRect.size = size;
    
    // If the single image draw covers the whole area, then just draw once.
    if (NSContainsRect(oneTileRect, rect)) {
        NSRect fromRect;
        fromRect.origin.x = rect.origin.x - oneTileRect.origin.x;
        fromRect.origin.y = (oneTileRect.origin.y + oneTileRect.size.height) - (rect.origin.y + rect.size.height);
        fromRect.size = rect.size;
        
        [self drawClippedToValidInRect:rect fromRect:fromRect];
        return;
    }
    
    // If we only have a partial image, just do nothing, because CoreGraphics will not help us tile
    // with a partial image. But maybe later we can fix this by constructing a pattern image that's
    // transparent where needed.
    if (loadStatus != NSImageRepLoadStatusCompleted) {
        return;
    }
    
    // Since we need to tile, construct an NSColor so we can get CoreGraphics to do it for us.
    if (patternColorLoadStatus != loadStatus) {
        [patternColor release];
        patternColor = nil;
    }
    if (patternColor == nil) {
        patternColor = [[NSColor colorWithPatternImage:self] retain];
        patternColorLoadStatus = loadStatus;
    }
    
    // Compute the appropriate phase relative to the top level view in the window.
    // Conveniently, the oneTileRect we computed above has the appropriate origin.
    NSPoint originInWindow = [[NSView focusView] convertPoint:oneTileRect.origin toView:nil];
    CGSize phase = CGSizeMake(fmodf(originInWindow.x, size.width), fmodf(originInWindow.y, size.height));
    
    [NSGraphicsContext saveGraphicsState];
    
    CGContextSetPatternPhase((CGContextRef)[[NSGraphicsContext currentContext] graphicsPort], phase);    
    [patternColor set];
    [NSBezierPath fillRect:rect];
    
    [NSGraphicsContext restoreGraphicsState];
}

// required by protocol -- apparently inherited methods don't count

- (NSSize)size
{
    return [super size];
}

@end
