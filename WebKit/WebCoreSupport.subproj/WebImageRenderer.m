/*	
        WebImageRenderer.m
	Copyright (c) 2002, 2003, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebImageRenderer.h>

#import <WebCore/WebCoreImageRenderer.h>
#import <WebFoundation/WebAssertions.h>

extern NSString *NSImageLoopCount;

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

#if MAC_OS_X_VERSION_MAX_ALLOWED <= MAC_OS_X_VERSION_10_2

// Part of the workaround for bug 3090341.
- (BOOL)blockHasGIFExtensionSignature:(const char *)block length:(int)length
{
    int i;
    for (i = 0; i < length - 10; i++) {
        if (block[i + 9] == '.' && block[i + 10] == '0') {
            if (memcmp(block + i, "NETSCAPE2", 9) == 0 || memcmp(block + i, "ANIMEXTS1", 9) == 0) {
                return YES;
            }
        }
    }
    return NO;
}

// Part of the workaround for bug 3090341.
- (void)checkDataForGIFExtensionSignature:(NSData *)data
{
    if (sawGIFExtensionSignature) {
        return;
    }
    
    const char *dataBytes = [data bytes];
    int dataLength = [data length];
    
    if (GIFExtensionBufferLength) {
        char leadingEdgeBuffer[20];
        memcpy(leadingEdgeBuffer, GIFExtensionBuffer, GIFExtensionBufferLength);
        int dataForLeadingEdgeBufferLength = dataLength;
        if (dataForLeadingEdgeBufferLength > 10) {
            dataForLeadingEdgeBufferLength = 10;
        }
        memcpy(leadingEdgeBuffer + GIFExtensionBufferLength, dataBytes, dataForLeadingEdgeBufferLength);
        if ([self blockHasGIFExtensionSignature:leadingEdgeBuffer
                length:GIFExtensionBufferLength + dataForLeadingEdgeBufferLength]) {
            sawGIFExtensionSignature = YES;
            return;
        }
    }
    
    if ([self blockHasGIFExtensionSignature:dataBytes length:dataLength]) {
        sawGIFExtensionSignature = YES;
        return;
    }
    
    if (dataLength < 10) {
        int keepLength = 10 - dataLength;
        if (keepLength > GIFExtensionBufferLength) {
            keepLength = GIFExtensionBufferLength;
        }
        memmove(GIFExtensionBuffer + GIFExtensionBufferLength - keepLength, GIFExtensionBuffer, keepLength);
        memcpy(GIFExtensionBuffer + keepLength, dataBytes, dataLength);
        GIFExtensionBufferLength = keepLength + dataLength;
    } else {
        memcpy(GIFExtensionBuffer, dataBytes + dataLength - 10, 10);
        GIFExtensionBufferLength = 10;
    }
}

#endif

- (id)initWithMIMEType:(NSString *)MIME
{
    [super init];
    MIMEType = [MIME copy];
    return self;
}

// Part of the workaround for bug 3090341.
- (id)initWithData:(NSData *)data MIMEType:(NSString *)MIME
{
#if MAC_OS_X_VERSION_MAX_ALLOWED <= MAC_OS_X_VERSION_10_2
    [self checkDataForGIFExtensionSignature:data];
#endif
    [super initWithData:data];
    MIMEType = [MIME copy];
    return self;
}


- copyWithZone:(NSZone *)zone
{
    WebImageRenderer *copy = [super copyWithZone:zone];
    
    // FIXME: If we copy while doing an incremental load, it won't work.
    
    copy->frameTimer = nil;
    copy->frameView = nil;
    copy->patternColor = nil;
    copy->MIMEType = [MIMEType copy];
        
    return copy;
}


- (BOOL)incrementalLoadWithBytes:(const void *)bytes length:(unsigned)length complete:(BOOL)isComplete
{
    NSBitmapImageRep *imageRep = [[self representations] objectAtIndex:0];
    NSData *data = [[NSData alloc] initWithBytes:bytes length:length];
    NSSize size;


#if MAC_OS_X_VERSION_MAX_ALLOWED <= MAC_OS_X_VERSION_10_2
    // Part of the workaround for bug 3090341.
    [self checkDataForGIFExtensionSignature:data];
#endif
    
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
        //printf ("incrementalLoadWithBytes: size %d, isComplete %d\n", length, isComplete);
        // We have some data.  Return YES so we can attempt to draw what we've got.
        return YES;
    }
    
    return NO;
}

- (void)dealloc
{
    ASSERT(frameTimer == nil);
    ASSERT(frameView == nil);
    [patternColor release];
    [MIMEType release];
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
    return property ? [property floatValue] : 0.0;
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

- (int)repetitionCount
{
    id property = [self firstRepProperty:NSImageLoopCount];
    int count = property ? [property intValue] : 0;
    // This is a workaround for bug 3090341.
    // If we see no extension, then the loop count is 1, not 0.
    if (count == 0 && !sawGIFExtensionSignature) {
        count = 1;
    }
    return count;
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
            // Figure out how much of the image is OK to draw.  We can't simply
            // use loadStatus because the image may be scaled.
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
            ASSERT([self isFlipped]);
            ASSERT([[NSView focusView] isFlipped]);
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
        repetitionsComplete += 1;
        if ([self repetitionCount] && repetitionsComplete >= [self repetitionCount]) {
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
            [self drawClippedToValidInRect:targetRect fromRect:imageRect];
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

    // Compute the appropriate phase relative to the top level view in the window.
    // Conveniently, the oneTileRect we computed above has the appropriate origin.
    NSPoint originInWindow = [[NSView focusView] convertPoint:oneTileRect.origin toView:nil];
    CGSize phase = CGSizeMake(fmodf(originInWindow.x, size.width), fmodf(originInWindow.y, size.height));
    
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
        
    [NSGraphicsContext saveGraphicsState];
    
    CGContextSetPatternPhase((CGContextRef)[[NSGraphicsContext currentContext] graphicsPort], phase);    
    [patternColor set];
    [NSBezierPath fillRect:rect];
    
    [NSGraphicsContext restoreGraphicsState];
}

- (void)resize:(NSSize)s
{
    [self setScalesWhenResized:YES];
    [self setSize:s];
}

// required by protocol -- apparently inherited methods don't count

- (NSSize)size
{
    return [super size];
}

@end
