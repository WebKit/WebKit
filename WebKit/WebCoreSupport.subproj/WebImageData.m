/*	
        WebImageData.m
	Copyright (c) 2004 Apple, Inc. All rights reserved.
*/
#import <WebKit/WebAssertions.h>
#import <WebKit/WebGraphicsBridge.h>
#import <WebKit/WebImageData.h>
#import <WebKit/WebImageDecoder.h>
#import <WebKit/WebImageRenderer.h>
#import <WebKit/WebImageRendererFactory.h>
#import <WebKit/WebKitSystemBits.h>

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
- (int)_repetitionCount;
- (float)_frameDuration;
- (void)_stopAnimation;
- (void)_nextFrame;
- (CFDictionaryRef)_imageSourceOptions;
@end


@implementation WebImageData

+ (void)initialize
{
    [WebImageRendererFactory setShouldUseThreadedDecoding:(WebNumberOfCPUs() >= 2 ? YES : NO)];
}

- init
{
    self = [super init];
    
    if ([WebImageRendererFactory shouldUseThreadedDecoding])
        decodeLock = [[NSLock alloc] init];

    imageSource = CGImageSourceCreateIncremental ([self _imageSourceOptions]);
    
    return self;
}


- (void)_commonTermination
{
    ASSERT (!frameTimer);
    
    [self _invalidateImages];
        
    if (imageSource)
        CFRelease (imageSource); 
        
    if (animatingRenderers)
        CFRelease (animatingRenderers);

    free (frameDurations);
}

- (void)dealloc
{
    [decodeLock release];

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

            if (imageProperties[i])
                CFRelease (imageProperties[i]);
        }
        free (images);
        images = 0;
        free (imageProperties);
        imageProperties = 0;
    }
}

- (CFDictionaryRef)_imageSourceOptions
{
    if (!imageSourceOptions) {
        CFStringRef keys[1] = { kCGImageSourceShouldCache };
        CFBooleanRef values[1] = { kCFBooleanTrue };
        imageSourceOptions = CFDictionaryCreate (NULL, (const void **)&keys, (const void **)&values, 1, 
                &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    }
    return imageSourceOptions;
}

- (CGImageRef)_noColorCorrectionImage:(CGImageRef)image withProperties:(CFDictionaryRef)props;
{
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
}
	    
- (CGImageRef)imageAtIndex:(size_t)index
{
    if (index >= imagesSize)
        return 0;

    return images[index];
}

- (CFDictionaryRef)propertiesAtIndex:(size_t)index
{
    if (index >= imagesSize)
        return 0;

    return imageProperties[index];
}

- (void)_createImages
{
    size_t i;

    [self _invalidateImages];
    
    imagesSize = [self numberOfImages];
    for (i = 0; i < imagesSize; i++) {
        if (!images) {
            images = (CGImageRef *)calloc (imagesSize, sizeof(CGImageRef *));
        }
            
        images[i] = CGImageSourceCreateImageAtIndex (imageSource, i, [self _imageSourceOptions]);

        if (!imageProperties) {
            imageProperties = (CFDictionaryRef *)malloc (imagesSize * sizeof(CFDictionaryRef));
        }
        
        imageProperties[i] = CGImageSourceGetPropertiesAtIndex (imageSource, i, 0);
        if (imageProperties[i])
            CFRetain (imageProperties[i]);
    }
}

// Called from decoder thread.
- (void)decodeData:(CFDataRef)data isComplete:(BOOL)f callback:(id)callback
{
    [decodeLock lock];
    
    CGImageSourceUpdateData (imageSource, data, f);

    // The work of decoding is actually triggered by image creation.
    [self _createImages];

    [decodeLock unlock];

    // Use status from first image to trigger appropriate notification back to WebCore
    // on main thread.
    if (callback) {
        CGImageSourceStatus imageStatus = CGImageSourceGetStatusAtIndex(imageSource, 0);
        
        // Lie about status.  If we have all the data, go ahead and say we're complete
        // as long we are have at least some valid bands (i.e. >= kCGImageStatusIncomplete).
        // We have to lie because CG incorrectly reports the status.
        if (f && imageStatus >= kCGImageStatusIncomplete)
            imageStatus = kCGImageStatusComplete;
        
        // Only send bad status if we've read the whole image.
        if (f || (!f && imageStatus >= kCGImageStatusIncomplete))
            [WebImageDecoder decodeComplete:callback status:imageStatus];
    }
}

- (BOOL)incrementalLoadWithBytes:(const void *)bytes length:(unsigned)length complete:(BOOL)isComplete callback:(id)callback
{
    CFDataRef data = CFDataCreate (NULL, bytes, length);
    
    if (callback) {
        [WebImageDecoder performDecodeWithImage:self data:data isComplete:isComplete callback:callback];
    }
    else {
        CGImageSourceUpdateData (imageSource, data, isComplete);
        [self _createImages];
    }
    
    CFRelease (data);

    return YES;
}

- (void)drawImageAtIndex:(size_t)index inRect:(CGRect)ir fromRect:(CGRect)fr adjustedSize:(CGSize)adjustedSize compositeOperation:(CGCompositeOperation)op context:(CGContextRef)aContext;
{    
    [decodeLock lock];
    
    CGImageRef image = [self imageAtIndex:index];
    
    if (!image) {
        [decodeLock unlock];
        return;
    }

    CGContextSaveGState (aContext);

    // Get the height of the portion of the image that is currently decoded.  This
    // could be less that the actual height.
    float h = CGImageGetHeight(image);

    // Is the amount of available bands less than what we need to draw?  If so,
    // clip.
    BOOL clipped = NO;
    CGSize actualSize = [self size];
    if (h != actualSize.height) {
	float proportionLoaded = h/actualSize.height;
	fr.size.height = fr.size.height * proportionLoaded;
	ir.size.height = ir.size.height * proportionLoaded;
	clipped = YES;
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
    if (clipped == NO && (fr.size.width != adjustedSize.width || fr.size.height != adjustedSize.height)) {
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

    [decodeLock unlock];
}

- (void)drawImageAtIndex:(size_t)index inRect:(CGRect)ir fromRect:(CGRect)fr compositeOperation:(CGCompositeOperation)op context:(CGContextRef)aContext;
{    
    [self drawImageAtIndex:index inRect:ir fromRect:fr adjustedSize:[self size] compositeOperation:op context:aContext];
}

static void drawPattern (void * info, CGContextRef context)
{
    WebImageData *data = (WebImageData *)info;
    
    CGImageRef image = (CGImageRef)[data imageAtIndex:[data currentFrame]];
    float w = CGImageGetWidth(image);
    float h = CGImageGetHeight(image);
    CGContextDrawImage (context, CGRectMake(0, 0, w, h), image);    
}

CGPatternCallbacks patternCallbacks = { 0, drawPattern, NULL };

- (void)tileInRect:(CGRect)rect fromPoint:(CGPoint)point context:(CGContextRef)aContext
{
    ASSERT (aContext);

    [decodeLock lock];
    
    CGImageRef image = [self imageAtIndex:[self currentFrame]];
    if (!image) {
        ERROR ("unable to find image");
        [decodeLock unlock];
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

        [decodeLock unlock];
        
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
    CGPatternRef pattern = CGPatternCreate(self, CGRectMake (0, 0, w, h), CGAffineTransformIdentity, w, h, 
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

    [decodeLock unlock];
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
        [decodeLock lock];
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
        [decodeLock unlock];
    }
    
    return size;
}

- (float)_floatProperty:(CFStringRef)property type:(CFStringRef)type at:(size_t)i
{
    [decodeLock lock];
    
    CFDictionaryRef properties = [self propertiesAtIndex:i];
    if (!properties) {
        [decodeLock unlock];
        return 0.f;
    }
    
    CFDictionaryRef typeProperties = CFDictionaryGetValue (properties, type);
    if (!typeProperties) {
        [decodeLock unlock];
        return 0.f;
    }
    
    // FIXME:  Use constant instead of DelayTime
    CFNumberRef num = CFDictionaryGetValue (typeProperties, property);
    if (!num) {
        [decodeLock unlock];
        return 0.f;
    }
    
    float value = 0.f;
    CFNumberGetValue (num, kCFNumberFloat32Type, &value);

    [decodeLock unlock];
    
    return value;
}

#define MINIMUM_DURATION (1.0/30.0)

- (float)_frameDurationAt:(size_t)i
{
    float duration = [self _floatProperty:kCGImagePropertyGIFDelayTime type:kCGImagePropertyGIFDictionary at:i];
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
        size_t i;

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
    return [self _floatProperty:kCGImagePropertyGIFLoopCount type:kCGImagePropertyGIFDictionary at:0];
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
        animatingRenderers = CFDictionaryCreateMutable (NULL, 0, NULL, &kCFTypeDictionaryValueCallBacks);
    
    NSMutableSet *renderers = (NSMutableSet *)CFDictionaryGetValue (animatingRenderers, view);
    if (!renderers) {
        renderers = [[NSMutableSet alloc] init];
        CFDictionaryAddValue(animatingRenderers, view, renderers);
	[renderers release];
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

- (BOOL)shouldAnimate
{
    return [self numberOfImages] > 1 && ![self isAnimationFinished];
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
