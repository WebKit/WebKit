/*	
        WebImageData.m
	Copyright (c) 2004 Apple, Inc. All rights reserved.
*/
#import <WebKit/WebAssertions.h>
#import <WebKit/WebGraphicsBridge.h>
#import <WebKit/WebImageData.h>
#import <WebKit/WebImageRenderer.h>
#import <WebKit/WebImageRendererFactory.h>

#import <WebCore/WebCoreImageRenderer.h>

#import <CoreGraphics/CGContextPrivate.h>
#import <CoreGraphics/CGContextGState.h>

#ifdef USE_CGIMAGEREF

static CFDictionaryRef imageSourceOptions;

// Forward declarations of internal methods.
@interface WebImageData (WebInternal)
- (void)_commonTermination;
- (void)_invalidateImages;
- (int)_repetitionCount;
- (float)_frameDuration;
- (void)_stopAnimation;
- (void)_nextFrame;
@end

@implementation WebImageData

- (void)_commonTermination
{
    ASSERT (!frameTimer);
    
    [self _invalidateImages];
    
    if (imageSource)
        CFRelease (imageSource); 
        
    if (animatingRenderers)
        CFRelease (animatingRenderers);
}

- (void)dealloc
{
    [self _commonTermination];

    [super dealloc];
}

- (void)finalize
{
    [self _commonTermination];
    [super finalize];
}

- copyWithZone:(NSZone *)zone
{
    WebImageData *copy;

    copy = [[WebImageData alloc] init];
    CFRetain (imageSource);
    copy->imageSource = imageSource;
    
    return copy;
}


- (size_t)numberOfImages
{
    if (imageSource)
        return CGImageSourceGetCount(imageSource);
    return 0;
}

- (size_t)currentFrame
{
    return currentFrame;
}

- (void)_invalidateImages
{
    if (images) {
        size_t i, count = [self numberOfImages];
        for (i = 0; i < count; i++) {
            if (images[i])
                CFRelease (images[i]);
        }
        free (images);
        images = 0;
    }
}

- (CGImageRef)imageAtIndex:(size_t)index
{
    if (imageSource) {
        if (index > [self numberOfImages])
            return 0;

        CGImageSourceStatus containerStatus = CGImageSourceGetStatus(imageSource);
        if (containerStatus < kCGImageStatusIncomplete)
            return 0;

        CGImageSourceStatus imageStatus = CGImageSourceGetStatusAtIndex(imageSource, index);
        if (imageStatus < kCGImageStatusIncomplete)
            return 0;

        if (!images) {
            images = (CGImageRef *)calloc ([self numberOfImages], sizeof(CGImageRef *));
        }
            
        if (!images[index]) {
            if (!imageSourceOptions) {
                CFStringRef keys[1] = { kCGImageSourceShouldCache };
                CFBooleanRef values[1] = { kCFBooleanTrue };
                imageSourceOptions = CFDictionaryCreate (NULL, (const void **)&keys, (const void **)&values, 1, 
                            &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
            }
            images[index] = CGImageSourceCreateImageAtIndex (imageSource, index, imageSourceOptions);
            if (images[index] == 0)
                ERROR ("unable to create image at index %d, containerStatus %d, image status %d", (int)index, containerStatus, imageStatus);
        }
        return images[index];
    }
    return 0;
}

- (BOOL)incrementalLoadWithBytes:(const void *)bytes length:(unsigned)length complete:(BOOL)isComplete
{
    if (!imageSource)
        imageSource = CGImageSourceCreateIncremental (imageSourceOptions);

    [self _invalidateImages];

    CFDataRef data = CFDataCreate (NULL, bytes, length);
    CGImageSourceUpdateData (imageSource, data, isComplete);
    CFRelease (data);
    
    CGImageSourceStatus status = CGImageSourceGetStatus(imageSource);
    
    if (status < kCGImageStatusIncomplete) {
        ERROR ("error update incremental image data, status %d", status);
    }
    
    return status >= kCGImageStatusIncomplete;
}

- (void)drawImageAtIndex:(size_t)index inRect:(CGRect)ir fromRect:(CGRect)fr compositeOperation:(CGCompositeOperation)op context:(CGContextRef)aContext;
{
    CGImageRef image = [self imageAtIndex:index];
    
    if (!image)
        return;

    CGContextSaveGState (aContext);

    float w = CGImageGetWidth(image);
    float h = CGImageGetHeight(image);

    // FIXME:  Need to determine number of available lines.
    int numCompleteLines = h;

    int pixelsHigh = h;
    if (pixelsHigh > numCompleteLines) {
        // Figure out how much of the image is OK to draw.  We can't simply
        // use numCompleteLines because the image may be scaled.
        float clippedImageHeight = floor([self size].height * numCompleteLines / pixelsHigh);
        
        // Figure out how much of the source is OK to draw from.
        float clippedSourceHeight = clippedImageHeight - fr.origin.y;
        if (clippedSourceHeight < 1) {
            return;
        }
        
        // Figure out how much of the destination we are going to draw to.
        float clippedDestinationHeight = ir.size.height * clippedSourceHeight / fr.size.height;

        // Reduce heights of both rectangles without changing their positions.
        // In the flipped case, just adjusting the height is sufficient.
        ir.size.height = clippedDestinationHeight;
        fr.size.height = clippedSourceHeight;
    }

    CGContextSetCompositeOperation (aContext, op);
    CGContextTranslateCTM (aContext, ir.origin.x, ir.origin.y);
    CGContextScaleCTM (aContext, 1, -1);
    CGContextTranslateCTM (aContext, 0, -h);
    
    // Translated to origin, now draw at 0,0.
    ir.origin.x = ir.origin.y = 0;
        
    if (fr.size.width != w || fr.size.height != h) {
        //image = CGImageCreateWithImageInRect (image, fr);
        //if (image) {
        //    CGContextDrawImage (aContext, ir, image);
        //    CFRelease (image);
        //}
        
        // FIXME:  Until the API above is implemented (CGImageCreateWithImageInRect), we
        // must create our own subimage.
        //
        // Create a new bitmap of the appropriate size and then draw that into our context.
        // Slo, boo!
        CGContextRef clippedSourceContext;
        CGImageRef clippedSourceImage;
        size_t csw = (size_t)fr.size.width;
        size_t csh = (size_t)fr.size.height;
                            
        CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
        size_t numComponents = CGColorSpaceGetNumberOfComponents(colorSpace);
        size_t bytesPerRow = ((csw * 8 * (numComponents+1) + 7)/8); // + 1 for alpha
        void *_drawingContextData = malloc(csh * bytesPerRow);
        
        // 8 bit per component
        clippedSourceContext = CGBitmapContextCreate(_drawingContextData, csw, csh, 8, bytesPerRow, colorSpace, kCGImageAlphaPremultipliedLast);
        CGContextTranslateCTM (clippedSourceContext, -fr.origin.x, -fr.origin.y);
        CGContextDrawImage (clippedSourceContext, CGRectMake(0,0,w,h), image);
        clippedSourceImage = CGBitmapContextCreateImage(clippedSourceContext);
        CGContextDrawImage (aContext, ir, clippedSourceImage);
        
        CGImageRelease (clippedSourceImage);
        CGContextRelease (clippedSourceContext);
        free (_drawingContextData);
    }
    else { 
        CGContextDrawImage (aContext, ir, image);
    }

    CGContextRestoreGState (aContext);
}

static void drawPattern (void * info, CGContextRef context)
{
    CGImageRef image = (CGImageRef)info;
    float w = CGImageGetWidth(image);
    float h = CGImageGetHeight(image);
    CGContextDrawImage (context, CGRectMake(0, 0, w, h), image);    
}

CGPatternCallbacks patternCallbacks = { 0, drawPattern, NULL };

- (void)tileInRect:(CGRect)rect fromPoint:(CGPoint)point context:(CGContextRef)aContext
{
    ASSERT (aContext);
    
    CGImageRef image = [self imageAtIndex:[self currentFrame]];
    if (!image) {
        ERROR ("unable to find image");
        return;
    }

    float w = CGImageGetWidth(image);
    float h = CGImageGetHeight(image);

    // Check and see if a single draw of the image can cover the entire area we are supposed to tile.
    NSRect oneTileRect;
    oneTileRect.origin.x = rect.origin.x + fmodf(fmodf(-point.x, w) - w, w);
    oneTileRect.origin.y = rect.origin.y + fmodf(fmodf(-point.y, h) - h, h);
    oneTileRect.size.width = w;
    oneTileRect.size.height = h;

    // If the single image draw covers the whole area, then just draw once.
    if (NSContainsRect(oneTileRect, NSMakeRect(rect.origin.x, rect.origin.y, rect.size.width, rect.size.height))) {
        CGRect fromRect;

        fromRect.origin.x = rect.origin.x - oneTileRect.origin.x;
        fromRect.origin.y = rect.origin.y - oneTileRect.origin.y;
        fromRect.size = rect.size;
        
        [self drawImageAtIndex:[self currentFrame] inRect:rect fromRect:fromRect compositeOperation:kCGCompositeSover context:aContext];
        return;
    }

    // Compute the appropriate phase relative to the top level view in the window.
    // Conveniently, the oneTileRect we computed above has the appropriate origin.
    NSPoint originInWindow = [[NSView focusView] convertPoint:oneTileRect.origin toView:nil];

    // WebCore may translate the focus, and thus need an extra phase correction
    NSPoint extraPhase = [[WebGraphicsBridge sharedBridge] additionalPatternPhase];
    originInWindow.x += extraPhase.x;
    originInWindow.y += extraPhase.y;
    CGSize phase = CGSizeMake(fmodf(originInWindow.x, w), fmodf(originInWindow.y, h));

    // Possible optimization:  We may want to cache the CGPatternRef    
    CGPatternRef pattern = CGPatternCreate(image, CGRectMake (0, 0, w, h), CGAffineTransformIdentity, w, h, 
        kCGPatternTilingConstantSpacing, true, &patternCallbacks);
    if (pattern) {
        CGContextSaveGState (aContext);

        CGContextSetPatternPhase(aContext, phase);

        CGColorSpaceRef patternSpace = CGColorSpaceCreatePattern(NULL);
        CGContextSetFillColorSpace(aContext, patternSpace);
        CGColorSpaceRelease(patternSpace);

        float patternAlpha = 1;
        CGContextSetFillPattern(aContext, pattern, &patternAlpha);

        CGContextSetCompositeOperation (aContext, kCGCompositeSover);

        CGContextFillRect (aContext, rect);

        CGPatternRelease (pattern);

        CGContextRestoreGState (aContext);
    }
    else {
        ERROR ("unable to create pattern");
    }
}

- (BOOL)isNull
{
    CGImageSourceStatus status = CGImageSourceGetStatusAtIndex(imageSource, [self currentFrame]);
    return status < kCGImageStatusIncomplete;
}

- (CGSize)size
{
    float w = 0.f, h = 0.f;
    CGImageRef image = [self imageAtIndex:0];
    if (image) {
        h = CGImageGetHeight(image);
        w = CGImageGetWidth(image);
    }
    return CGSizeMake(w,h);
}

#define MINIMUM_DURATION (1.0/30.0)

- (float)_frameDuration
{
    CFDictionaryRef properties = CGImageSourceGetPropertiesAtIndex (imageSource, currentFrame, 0);
    if (!properties)
        return 0.f;
        
    // FIXME:  Use constant instead of {GIF}
    CFDictionaryRef GIFProperties = CFDictionaryGetValue (properties, @"{GIF}");
    if (!GIFProperties)
        return 0.f;
    
    // FIXME:  Use constant instead of DelayTime
    CFNumberRef num = CFDictionaryGetValue (GIFProperties, @"DelayTime");
    if (!num)
        return 0.f;
    
    float duration = 0.f;
    CFNumberGetValue (num, kCFNumberFloat32Type, &duration);
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

- (int)_repetitionCount
{
    // FIXME:  Need to get this from CG folks.
    return 0;
}

- (BOOL)isAnimationFinished
{
    return animationFinished;
}

static NSMutableSet *activeAnimations;

+ (void)stopAnimationsInView:(NSView *)aView
{
    NSEnumerator *objectEnumerator = [activeAnimations objectEnumerator];
    WebImageData *animation;
    NSMutableSet *animationsToStop = [[NSMutableSet alloc] init];

    while ((animation = [objectEnumerator nextObject])) {
        if (CFDictionaryGetValue (animation->animatingRenderers, aView)) {
            [animationsToStop addObject: animation];
        }
    }

    objectEnumerator = [animationsToStop objectEnumerator];
    while ((animation = [objectEnumerator nextObject])) {
        CFDictionaryRemoveValue (animation->animatingRenderers, aView);
        if (CFDictionaryGetCount(animation->animatingRenderers) == 0) {
            [animation _stopAnimation];
        }
    }
    [animationsToStop release];
}

- (void)addAnimatingRenderer:(WebImageRenderer *)r inView:(NSView *)view
{
    if (!animatingRenderers)
        animatingRenderers = CFDictionaryCreateMutable (NULL, 0, NULL, NULL);
    
    NSMutableSet *renderers = (NSMutableSet *)CFDictionaryGetValue (animatingRenderers, view);
    if (!renderers) {
        renderers = [[NSMutableSet alloc] init];
        CFDictionaryAddValue(animatingRenderers, view, renderers);
    }
            
    [renderers addObject:r];

    if (!activeAnimations)
        activeAnimations = [[NSMutableSet alloc] init];
    [activeAnimations addObject:self];
}

- (void)removeAnimatingRenderer:(WebImageRenderer *)r
{
    NSEnumerator *viewEnumerator = [(NSMutableDictionary *)animatingRenderers keyEnumerator];
    NSView *view;
    while ((view = [viewEnumerator nextObject])) {
        NSMutableSet *renderers = (NSMutableSet *)CFDictionaryGetValue (animatingRenderers, view);
        [renderers removeObject:r];
        if ([renderers count] == 0) {
            CFDictionaryRemoveValue (animatingRenderers, renderers);
        }
    }
    
    if (animatingRenderers && CFDictionaryGetCount(animatingRenderers) == 0) {
        [activeAnimations removeObject:self];
    }
}

- (void)_stopAnimation
{
    [frameTimer invalidate];
    [frameTimer release];
    frameTimer = nil;
}

- (void)_nextFrame:(id)context
{
    // Release the timer that just fired.
    [frameTimer release];
    frameTimer = nil;
    
    currentFrame++;
    if (currentFrame >= [self numberOfImages]) {
        repetitionsComplete += 1;
        if ([self _repetitionCount] && repetitionsComplete >= [self _repetitionCount]) {
            animationFinished = YES;
            return;
        }
        currentFrame = 0;
    }
    
    NSEnumerator *viewEnumerator = [(NSMutableDictionary *)animatingRenderers keyEnumerator];
    NSView *view;
    while ((view = [viewEnumerator nextObject])) {
        NSMutableSet *renderers = [(NSMutableDictionary *)animatingRenderers objectForKey:view];
        WebImageRenderer *renderer;
        NSEnumerator *rendererEnumerator = [renderers objectEnumerator];
        while ((renderer = [rendererEnumerator nextObject])) {
            [view setNeedsDisplayInRect:[renderer targetAnimationRect]];
        }
    }
}

- (void)animate
{
    if (frameTimer && [frameTimer isValid])
        return;
    frameTimer = [[NSTimer scheduledTimerWithTimeInterval:[self _frameDuration]
                                                    target:self
                                                  selector:@selector(_nextFrame:)
                                                  userInfo:nil
                                                   repeats:NO] retain];
}

@end

#endif
