/*	
        IFImageRenderer.m
	Copyright (c) 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/IFException.h>
#import <WebKit/IFImageRenderer.h>
#import <WebKit/WebKitDebug.h>

@implementation IFImageRenderer

static NSMutableArray *activeImageRenderers;

+ (void)stopAnimationsInView: (NSView *)aView
{
    int i, count;    

    count = [activeImageRenderers count];
    for (i = count-1; i >= 0; i--){
        IFImageRenderer *renderer = [activeImageRenderers objectAtIndex: i];
        if (renderer->frameView == aView){
            [renderer stopAnimation];
        }
    }
    
}

- initWithSize:(NSSize)size
{
    self = [super initWithSize:size];
    
    statusOfCache = NSImageRepLoadStatusUnknownType;
    
    return self;
}


- copyWithZone:(NSZone *)zone
{
    IFImageRenderer *copy = [super copyWithZone:zone];
    
    copy->frameTimer = nil;
    copy->frameView = nil;
    copy->patternColor = nil;
    
    return copy;
}

- (BOOL)incrementalLoadWithBytes: (const void *)bytes length:(unsigned)length complete:(BOOL)isComplete
{
    NSBitmapImageRep* imageRep = [[self representations] objectAtIndex:0];
    //NSData *data = [[NSData alloc] initWithBytesNoCopy: (void *)bytes length: length freeWhenDone: NO];
    NSData *data = [[NSData alloc] initWithBytes: (void *)bytes length: length];
    
    loadStatus = [imageRep incrementalLoadFromData:data complete:isComplete];
    [data release];
    switch (loadStatus){
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
        [imageRep setSize:NSMakeSize([imageRep pixelsWide], [imageRep pixelsHigh])];
        return YES;
    default:
        // We have some data.  Return YES so we can attempt a to draw what we've got.
        return YES;
    }
    
    return NO;
}


- (int)loadStatus
{
    return loadStatus;
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
    NSWindow *window;
    
    // Release the timer that just fired.
    [frameTimer release];
    frameTimer = nil;
    
    currentFrame = [self currentFrame] + 1;
    if (currentFrame >= [self frameCount]) {
        currentFrame = 0;
    }
    [self setCurrentFrame:currentFrame];
    
    window = [frameView window];
    
     // We can't use isOpaque because it returns YES for many non-opaque images (Radar 2966937).
     // But we can at least assume that any image representatin without alpha is opaque.
     if (![[[self representations] objectAtIndex:0] hasAlpha]) {
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
    } else {
        [frameView displayRect:targetRect];
    }
    
    [window flushWindow];
}

- (void)beginAnimationInRect: (NSRect)ir fromRect: (NSRect)fr
{
    // The previous, if any, frameView, is released in stopAnimation.
    [self stopAnimation];
    
    if ([self frameCount] > 1) {
        imageRect = fr;
        targetRect = ir;
        frameView = [[NSView focusView] retain];
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

- (void)tileInRect:(NSRect)rect fromPoint:(NSPoint)point
{
    int currentStatus = [self loadStatus];
    
    if (currentStatus > 0 || currentStatus == NSImageRepLoadStatusCompleted){
        if (statusOfCache != currentStatus){
            [patternColor release];
            patternColor = [[NSColor colorWithPatternImage:self] retain];
            statusOfCache = currentStatus;
        }
        
        // FIXME: This doesn't use the passed in point to determine the pattern phase.
        // It might be OK to do what we're doing, but I'm not 100% sure.
        // This code uses the coordinate system of whatever converting toView:nil
        // does, which may be OK.
        NSPoint p = [[NSView focusView] convertPoint:rect.origin toView:nil];
        NSSize size = [self size];
        CGSize phase = { (int)p.x % (int)size.width, (int)p.y % (int)size.height };
        
        [NSGraphicsContext saveGraphicsState];
        
        CGContextRef cgContext = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];
        CGContextSetPatternPhase(cgContext, phase);
        [patternColor set];
        
        [NSBezierPath fillRect:rect];
        
        [NSGraphicsContext restoreGraphicsState];
    }
}

// required by protocol -- apparently inherited methods don't count

- (NSSize)size
{
    return [super size];
}

@end
