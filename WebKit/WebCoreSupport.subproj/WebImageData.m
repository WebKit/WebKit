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
#import <CoreGraphics/CGColorSpacePrivate.h>

#ifdef USE_CGIMAGEREF

static CFDictionaryRef imageSourceOptions;

// Forward declarations of internal methods.
@interface WebImageData (WebInternal)
- (void)_commonTermination;
- (void)_invalidateImages;
- (void)_invalidateImageProperties;
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
    
    [self _invalidateImageProperties];
    
    if (imageSource)
        CFRelease (imageSource); 
        
    if (animatingRenderers)
        CFRelease (animatingRenderers);

    free (frameDurations);
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
        size_t i;
        for (i = 0; i < imagesSize; i++) {
            if (images[i])
                CFRelease (images[i]);
        }
        free (images);
        images = 0;
    }
}

- (void)_invalidateImageProperties
{
    size_t i;
    for (i = 0; i < imagePropertiesSize; i++) {
	if (imageProperties[i])
	    CFRelease (imageProperties[i]);
    }
    free (imageProperties);
    imageProperties = 0;
    imagePropertiesSize = 0;
}

- (CGImageRef)_noColorCorrectionImage:(CGImageRef)image withProperties:(CFDictionaryRef)props;
{
#if NO_COLOR_CORRECTION
    CGColorSpaceRef uncorrectedColorSpace = 0;
    CGImageRef noColorCorrectionImage = 0;

    if (!CFDictionaryGetValue (props, kCGImagePropertyProfileName)) {
	CFStringRef colorModel = CFDictionaryGetValue (props, kCGImagePropertyColorModel);
	
	if (colorModel) {
	    if (CFStringCompare (colorModel, (CFStringRef)@"RGB", 0) == kCFCompareEqualTo)
		uncorrectedColorSpace = CGColorSpaceCreateDisplayRGB();
	    else if (CFStringCompare (colorModel, (CFStringRef)@"Gray", 0) == kCFCompareEqualTo)
		uncorrectedColorSpace = CGColorSpaceCreateDisplayGray();
	}
	
	if (uncorrectedColorSpace) {
	    noColorCorrectionImage = CGImageCreateCopyWithColorSpace (image, uncorrectedColorSpace);
	    CFRelease (uncorrectedColorSpace);
	}
    }
    return noColorCorrectionImage;
#else
    return 0;
#endif
}
	    
- (CGImageRef)imageAtIndex:(size_t)index
{
    size_t num = [self numberOfImages];
    
    if (imagesSize && num > imagesSize)
        [self _invalidateImages];
        
    if (imageSource) {
        if (index > [self numberOfImages])
            return 0;

#ifndef NDEBUG
        CGImageSourceStatus containerStatus = CGImageSourceGetStatus(imageSource);
#endif
	// Ignore status, just try to create the image!  Status reported from ImageIO 
	// is bogus until the image is created.  See 3827851
        //if (containerStatus < kCGImageStatusIncomplete)
        //    return 0;

#ifndef NDEBUG
        CGImageSourceStatus imageStatus = CGImageSourceGetStatusAtIndex(imageSource, index);
#endif
	// Ignore status.  Status is invalid until we create the image (and eventually try to display it).
	// See 3827851
        //if (imageStatus < kCGImageStatusIncomplete)
        //    return 0;

        imagesSize = [self numberOfImages];
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
	
#if NO_COLOR_CORRECTION
	if (imageStatus >= kCGImageStatusIncomplete) {
	    CGImageRef noColorCorrectionImage = [self _noColorCorrectionImage:images[index]
						withProperties:[self propertiesAtIndex:index]];
	    if (noColorCorrectionImage) {
		CFRelease (images[index]);
		images[index] = noColorCorrectionImage;
	    }
	}
#endif
	
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

    // Always returns YES because we can't rely on status.  See 3827851
    //CGImageSourceStatus status = CGImageSourceGetStatus(imageSource);
    //
    //return status >= kCGImageStatusReadingHeader;
    return YES;
}

- (void)drawImageAtIndex:(size_t)index inRect:(CGRect)ir fromRect:(CGRect)fr compositeOperation:(CGCompositeOperation)op context:(CGContextRef)aContext;
{    
    CGImageRef image = [self imageAtIndex:index];
    
    if (!image)
        return;

    CGContextSaveGState (aContext);

    //float w = CGImageGetWidth(image);
    float h = CGImageGetHeight(image);

    // Is the amount of available bands less than what we need to draw?  If so,
    // clip.
    BOOL clipping = NO;
    if (h < fr.size.height) {
	fr.size.height = h;
	ir.size.height = h;
	clipping = YES;
    }
    
    // Flip the coords.
    CGContextSetCompositeOperation (aContext, op);
    CGContextTranslateCTM (aContext, ir.origin.x, ir.origin.y);
    CGContextScaleCTM (aContext, 1, -1);
    CGContextTranslateCTM (aContext, 0, -fr.size.height);
    
    // Translated to origin, now draw at 0,0.
    ir.origin.x = ir.origin.y = 0;
    
    // If we're drawing a sub portion of the image then create
    // a image for the sub portion and draw that.
    // Test using example site at http://www.meyerweb.com/eric/css/edge/complexspiral/demo.html
    if (fr.origin.x != 0 || fr.origin.y != 0 || 
	fr.size.width != ir.size.width || fr.size.height != fr.size.height ||
	clipping) {
        image = CGImageCreateWithImageInRect (image, fr);
        if (image) {
            CGContextDrawImage (aContext, ir, image);
            CFRelease (image);
        }
    }
    // otherwise draw the whole image.
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
    if (imageSource)
	return CGImageSourceGetStatus(imageSource) < kCGImageStatusReadingHeader;
    return YES;
}

- (CGSize)size
{
    float w = 0.f, h = 0.f;

    if (!haveSize) {
        CFDictionaryRef properties = [self propertiesAtIndex:0];
        if (properties) {
            CFNumberRef num = CFDictionaryGetValue (properties, kCGImagePropertyPixelWidth);
            if (num)
                CFNumberGetValue (num, kCFNumberFloat32Type, &w);
            num = CFDictionaryGetValue (properties, kCGImagePropertyPixelHeight);
            if (num)
                CFNumberGetValue (num, kCFNumberFloat32Type, &h);

            size.width = w;
            size.height = h;
            
            haveSize = YES;
        }
    }
    
    return size;
}

- (CFDictionaryRef)propertiesAtIndex:(size_t)index
{
    size_t num = [self numberOfImages];
    
    // Number of images changed!
    if (imagePropertiesSize && num > imagePropertiesSize) {
        // Clear cache.
	[self _invalidateImageProperties];
    }
    
    if (imageProperties == 0 && num) {
        imageProperties = (CFDictionaryRef *)malloc (num * sizeof(CFDictionaryRef));
        size_t i;
        for (i = 0; i < num; i++) {
            imageProperties[i] = CGImageSourceGetPropertiesAtIndex (imageSource, i, 0);
            if (imageProperties[i])
                CFRetain (imageProperties[i]);
        }
        imagePropertiesSize = num;
    }
    
    if (index < num) {
        // If image properties are nil, try to get them again.  May have attempted to
        // get them before enough data was available in the header.
        if (imageProperties[index] == 0) {
            imageProperties[index] = CGImageSourceGetPropertiesAtIndex (imageSource, index, 0);
            if (imageProperties[index])
                CFRetain (imageProperties[index]);
        }
        
        return imageProperties[index];
    }
        
    return 0;
}

#define MINIMUM_DURATION (1.0/30.0)

- (float)_frameDurationAt:(size_t)i
{
    CFDictionaryRef properties = [self propertiesAtIndex:i];
    if (!properties) {
        return 0.f;
    }
    
    // FIXME:  Use constant instead of {GIF}
    CFDictionaryRef GIFProperties = CFDictionaryGetValue (properties, @"{GIF}");
    if (!GIFProperties) {
        return 0.f;
    }
    
    // FIXME:  Use constant instead of DelayTime
    CFNumberRef num = CFDictionaryGetValue (GIFProperties, @"DelayTime");
    if (!num) {
        return 0.f;
    }
    
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

- (float)_frameDuration
{
    size_t num = [self numberOfImages];
    if (frameDurationsSize && num > frameDurationsSize) {
        free (frameDurations);
        frameDurations = 0;
        frameDurationsSize = 0;
    }
    
    if (!frameDurations) {
        size_t i, num = [self numberOfImages];

        frameDurations = (float *)malloc (sizeof(float) * num);
        for (i = 0; i < num; i++) {
            frameDurations[i] = [self _frameDurationAt:i];
        }
        frameDurationsSize = num;
    }
    else if (frameDurations[currentFrame] == 0.f) {
            frameDurations[currentFrame] = [self _frameDurationAt:currentFrame];
    }

    return frameDurations[currentFrame];
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
    NSMutableSet *renderersToStop = nil;

    // Determine all the renderers that are drawing animations in the view.
    // A set of sets, one set of renderers for each image being animated
    // in the view.  It is necessary to gather the all renderers to stop
    // before actually stopping them because the process of stopping them
    // will modify the active animations and animating renderer collections.
    NSSet *renderersInView;
    while ((animation = [objectEnumerator nextObject])) {
	renderersInView = (NSSet *)CFDictionaryGetValue (animation->animatingRenderers, aView);
        if (renderersInView) {
	    if (!renderersToStop)
		renderersToStop = [[NSMutableSet alloc] init];
            [renderersToStop addObject: renderersInView];
        }
    }

    // Now tell them all to stop drawing.
    if (renderersToStop) {
	objectEnumerator = [renderersToStop objectEnumerator];
	while ((renderersInView = [objectEnumerator nextObject])) {
	    [renderersInView makeObjectsPerformSelector:@selector(stopAnimation)];
	}
	
	[renderersToStop release];
    }
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
            CFDictionaryRemoveValue (animatingRenderers, view);
        }
    }
    
    if (animatingRenderers && CFDictionaryGetCount(animatingRenderers) == 0) {
        [activeAnimations removeObject:self];
	[self _stopAnimation];
    }
}

- (void)_stopAnimation
{
    // This timer is used to animate all occurences of this image.  Don't invalidate
    // the timer unless all renderers have stopped drawing.
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
