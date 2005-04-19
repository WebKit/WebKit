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

#import <ImageIO/CGImageSourcePrivate.h>

// Forward declarations of internal methods.
@interface WebImageData (WebInternal)
- (void)_commonTermination;
- (void)_invalidateImages;
- (void)_invalidateImageProperties;
- (int)_repetitionCount;
- (float)_frameDuration;
- (void)_stopAnimation;
- (void)_nextFrame;
- (CFDictionaryRef)_imageSourceOptions;
-(void)_createPDFWithData:(NSData *)data;
- (CGPDFDocumentRef)_PDFDocumentRef;
- (BOOL)_PDFDrawFromRect:(NSRect)srcRect toRect:(NSRect)dstRect operation:(CGCompositeOperation)op alpha:(float)alpha flipped:(BOOL)flipped context:(CGContextRef)context;
- (void)_cacheImages:(size_t)optionalIndex allImages:(BOOL)allImages;
@end


@implementation WebImageData

+ (void)initialize
{
    // Currently threaded decoding doesn't play well with the WebCore cache.  Until
    // those issues are resolved threaded decoding is OFF by default, even on dual CPU
    // machines.
    //[WebImageRendererFactory setShouldUseThreadedDecoding:(WebNumberOfCPUs() >= 2 ? YES : NO)];
    [WebImageRendererFactory setShouldUseThreadedDecoding:NO];
}

- init
{
    self = [super init];
    
    if ([WebImageRendererFactory shouldUseThreadedDecoding])
        decodeLock = [[NSLock alloc] init];

    imageSource = CGImageSourceCreateIncremental ([self _imageSourceOptions]);
    sizeAvailable = NO;
    
    return self;
}

- (void)setIsPDF:(BOOL)f
{
    isPDF = f;
}

- (BOOL)isPDF
{
    return isPDF;
}

- (void)_commonTermination
{
    ASSERT (!frameTimer);
    
    [self _invalidateImages];
    [self _invalidateImageProperties];
        
    if (fileProperties)
	CFRelease (fileProperties);
	
    if (imageSource)
        CFRelease (imageSource); 
        
    if (animatingRenderers)
        CFRelease (animatingRenderers);

    free (frameDurations);
}

- (void)dealloc
{
    [_PDFDoc release];
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
        }
        free (images);
        images = 0;
        
        isSolidColor = NO;
        if( solidColor ) {
            CFRelease(solidColor);
            solidColor = NULL;
        }
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

- (CFDictionaryRef)_imageSourceOptions
{
    static CFDictionaryRef imageSourceOptions;
    if (!imageSourceOptions) {
        const void * keys[2] = { kCGImageSourceShouldCache, kCGImageSourceShouldPreferRGB32 };
        const void * values[2] = { kCFBooleanTrue, kCFBooleanTrue };
        imageSourceOptions = CFDictionaryCreate (NULL, keys, values, 2, 
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
	    if (CFStringCompare (colorModel, CFSTR("RGB"), 0) == kCFCompareEqualTo)
		uncorrectedColorSpace = CGColorSpaceCreateDisplayRGB();
	    else if (CFStringCompare (colorModel, CFSTR("Gray"), 0) == kCFCompareEqualTo)
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
    if (index >= [self numberOfImages])
        return 0;

    if (!images || images[index] == 0){
	[self _cacheImages:index allImages:NO];
    }
    
    return images[index];
}

- (CFDictionaryRef)fileProperties
{
    if (!fileProperties) {
	fileProperties = CGImageSourceCopyProperties (imageSource, [self _imageSourceOptions]);
    }
    
    return fileProperties;
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
#if USE_DEPRECATED_IMAGESOURCE_API	
            imageProperties[i] = CGImageSourceGetPropertiesAtIndex (imageSource, i, [self _imageSourceOptions]);
            if (imageProperties[i])
                CFRetain (imageProperties[i]);
#else
            imageProperties[i] = CGImageSourceCopyPropertiesAtIndex (imageSource, i, [self _imageSourceOptions]);
#endif
        }
        imagePropertiesSize = num;
    }
    
    if (index < num) {
        // If image properties are nil, try to get them again.  May have attempted to
        // get them before enough data was available in the header.
        if (imageProperties[index] == 0) {
#if USE_DEPRECATED_IMAGESOURCE_API	
            imageProperties[index] = CGImageSourceGetPropertiesAtIndex (imageSource, index, [self _imageSourceOptions]);
            if (imageProperties[index])
                CFRetain (imageProperties[index]);
#else
            imageProperties[index] = CGImageSourceCopyPropertiesAtIndex (imageSource, index, [self _imageSourceOptions]);
#endif
        }
        
        return imageProperties[index];
    }
    
    return 0;
}

- (void)_checkSolidColor: (CGImageRef)image
{
    isSolidColor = NO;
    if( solidColor ) {
        CFRelease(solidColor);
        solidColor = NULL;
    }
    
    // Currently we only check for solid color in the important special case of a 1x1 image
    if( image && CGImageGetWidth(image)==1 && CGImageGetHeight(image)==1 ) {
        float pixel[4]; // RGBA
        CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
        CGContextRef bmap = CGBitmapContextCreate(&pixel,1,1,8*sizeof(float),sizeof(pixel),space,
                                                  kCGImageAlphaPremultipliedLast | kCGBitmapFloatComponents);
        if( bmap ) {
            CGContextSetCompositeOperation(bmap, kCGCompositeCopy);
            CGRect dst = {{0,0},{1,1}};
            CGContextDrawImage(bmap,dst,image);
            if( pixel[3] > 0 )
                solidColor = CGColorCreate(space,pixel);
            isSolidColor = YES;
            CFRelease(bmap);
            /*NSLog(@"WebImageData %p: 1x1 image has color {%f,%f,%f,%f} => %p",
                  self,pixel[0],pixel[1],pixel[2],pixel[3],solidColor);*/
        } else {
            ERROR("Couldn't create CGBitmapContext");
        }
        CFRelease(space);
    }
}

- (void)_cacheImages:(size_t)optionalIndex allImages:(BOOL)allImages
{
    size_t i;

    imagesSize = [self numberOfImages];
    size_t from, to;
    
    if (allImages) {
	from = 0;
	to = imagesSize;
    }
    else {
	from = optionalIndex;
	to = optionalIndex+1;
    }
    for (i = from; i < to; i++) {
        if (!images) {
            images = (CGImageRef *)calloc (imagesSize, sizeof(CGImageRef));
        }

        images[i] = CGImageSourceCreateImageAtIndex (imageSource, i, [self _imageSourceOptions]);
    }
    
    if (from==0 && to>0) {
        // Loaded image 0. Check whether it's a solid color:
        [self _checkSolidColor: images[0]];
    }
}

// Called from decoder thread.
- (void)decodeData:(CFDataRef)data isComplete:(BOOL)isComplete callback:(id)callback
{
    [decodeLock lock];
    
    if (isPDF) {
        if (isComplete) {
            [self _createPDFWithData:(NSData *)data];
        }
    }
    else {
        // The work of decoding is actually triggered by image creation.
        CGImageSourceUpdateData (imageSource, data, isComplete);
	[self _invalidateImages];
        [self _cacheImages:0 allImages:YES];
    }
        
    [decodeLock unlock];

    if (isPDF) {
        if (isComplete && callback) {
            if ([self _PDFDocumentRef]) {
                [WebImageDecoder decodeComplete:callback status:kCGImageStatusComplete];
            }
            else {
                [WebImageDecoder decodeComplete:callback status:kCGImageStatusInvalidData];
            }
        }
    }
    else {
        // Use status from first image to trigger appropriate notification back to WebCore
        // on main thread.
        if (callback) {
            CGImageSourceStatus imageStatus = CGImageSourceGetStatusAtIndex(imageSource, 0);
            
            // Lie about status.  If we have all the data, go ahead and say we're complete
            // as long we are have at least some valid bands (i.e. >= kCGImageStatusIncomplete).
            // We have to lie because CG incorrectly reports the status.
            if (isComplete && imageStatus >= kCGImageStatusIncomplete)
                imageStatus = kCGImageStatusComplete;
            
            // Only send bad status if we've read the whole image.
            if (isComplete || (!isComplete && imageStatus >= kCGImageStatusIncomplete))
                [WebImageDecoder decodeComplete:callback status:imageStatus];
        }
    }
}

- (BOOL)_isSizeAvailable
{
    CGImageSourceStatus imageSourceStatus = CGImageSourceGetStatus(imageSource);
    
    if (sizeAvailable)
        return YES;
        
    // With ImageIO-55 the meta data for an image is updated progressively.  We don't want
    // to indicate that the image is 'ready' for layout until we know the size.  So, we
    // have to try getting the meta until we have a valid width and height property.
    if (imageSourceStatus >= kCGImageStatusIncomplete) {
        CFDictionaryRef image0Properties = CGImageSourceCopyPropertiesAtIndex (imageSource, 0, [self _imageSourceOptions]);
        if  (image0Properties) {
            CFNumberRef widthNumber = CFDictionaryGetValue (image0Properties, kCGImagePropertyPixelWidth);
            CFNumberRef heightNumber = CFDictionaryGetValue (image0Properties, kCGImagePropertyPixelHeight);
            sizeAvailable = widthNumber && heightNumber;
            
            if (imageProperties) {
                if (imageProperties[0])
                    CFRelease(imageProperties[0]);
            }
            else {
                imagePropertiesSize = [self numberOfImages];
                imageProperties = (CFDictionaryRef *)calloc (imagePropertiesSize, sizeof(CFDictionaryRef));
            }
                
            imageProperties[0] = image0Properties;
        }
    }
    
    return sizeAvailable;
}

- (BOOL)incrementalLoadWithBytes:(const void *)bytes length:(unsigned)length complete:(BOOL)isComplete callback:(id)callback
{
#ifdef kImageBytesCutoff
    // This is a hack to help with testing display of partially-loaded images.
    // To enable it, define kImageBytesCutoff to be a size smaller than that of the image files
    // being loaded. They'll never finish loading.
    if( length > kImageBytesCutoff ) {
        length = kImageBytesCutoff;
        isComplete = NO;
    }
#endif
    
    CFDataRef data = CFDataCreate (NULL, bytes, length);
    
    if (callback) {
        [WebImageDecoder performDecodeWithImage:self data:data isComplete:isComplete callback:callback];
    }
    else {
        if (isPDF) {
            if (isComplete) {
                [self _createPDFWithData:(NSData *)data];
            }
        }
        else {
	    [self _invalidateImages];
            CGImageSourceUpdateData (imageSource, data, isComplete);
        }
    }
    
    CFRelease (data);

    // Image properties will not be available until the first frame of the file
    // reaches kCGImageStatusIncomplete.  New as of ImageIO-55, see 4031602. 
    if (imageSource) {
        return [self _isSizeAvailable];
    }
    
    return YES;
}

- (void)_fillSolidColorInRect:(CGRect)rect compositeOperation:(CGCompositeOperation)op context:(CGContextRef)aContext
{
    /*NSLog(@"WebImageData %p: filling with color %p, in {%.0f,%.0f, %.0f x %.0f}",
          self,solidColor,rect.origin.x,rect.origin.y,rect.size.width,rect.size.height);*/
    if( solidColor ) {
        CGContextSaveGState (aContext);
        CGContextSetFillColorWithColor(aContext, solidColor);
        CGContextSetCompositeOperation (aContext, op);
        CGContextFillRect (aContext, rect);
        CGContextRestoreGState (aContext);
    }
}

- (void)drawImageAtIndex:(size_t)index inRect:(CGRect)ir fromRect:(CGRect)fr adjustedSize:(CGSize)adjustedSize compositeOperation:(CGCompositeOperation)op context:(CGContextRef)aContext;
{
    if (isPDF) {
        [self _PDFDrawFromRect:NSMakeRect(fr.origin.x, fr.origin.y, fr.size.width, fr.size.height)
                toRect:NSMakeRect(ir.origin.x, ir.origin.y, ir.size.width, ir.size.height)
                operation:op 
                alpha:1.0 
                flipped:YES
                context:aContext];
    }
    else {
        [decodeLock lock];
        
        CGImageRef image = [self imageAtIndex:index];
        
        if (!image) {
            [decodeLock unlock];
            return;
        }
        
        if( isSolidColor && index==0 ) {
            [self _fillSolidColorInRect: ir compositeOperation: op context: aContext];

        } else {
            CGContextSaveGState (aContext);
            
            // Get the height (in adjusted, i.e. scaled, coords) of the portion of the image
            // that is currently decoded.  This could be less that the actual height.
            CGSize selfSize = [self size];                          // full image size, in pixels
            float curHeight = CGImageGetHeight(image);              // height of loaded portion, in pixels
            
            if( curHeight < selfSize.height ) {
                adjustedSize.height *= curHeight / selfSize.height;

                // Is the amount of available bands less than what we need to draw?  If so,
                // we may have to clip 'fr' if it goes outside the available bounds.
                if( CGRectGetMaxY(fr) > adjustedSize.height ) {
                    float frHeight = adjustedSize.height - fr.origin.y; // clip fr to available bounds
                    if( frHeight <= 0 ) {
                        [decodeLock unlock];
                        return;                                             // clipped out entirely
                    }
                    ir.size.height *= (frHeight / fr.size.height);    // scale ir proportionally to fr
                    fr.size.height = frHeight;
                }
            }
            
            // Flip the coords.
            CGContextSetCompositeOperation (aContext, op);
            CGContextTranslateCTM (aContext, ir.origin.x, ir.origin.y);
            CGContextScaleCTM (aContext, 1, -1);
            CGContextTranslateCTM (aContext, 0, -ir.size.height);
            
            // Translated to origin, now draw at 0,0.
            ir.origin.x = ir.origin.y = 0;
            
            // If we're drawing a sub portion of the image then create
            // a image for the sub portion and draw that.
            // Test using example site at http://www.meyerweb.com/eric/css/edge/complexspiral/demo.html
            if (fr.size.width != adjustedSize.width || fr.size.height != adjustedSize.height) {
                // Convert ft to image pixel coords:
                float xscale = adjustedSize.width / selfSize.width;
                float yscale = adjustedSize.height / curHeight;     // yes, curHeight, not selfSize.height!
                fr.origin.x /= xscale;
                fr.origin.y /= yscale;
                fr.size.width /= xscale;
                fr.size.height /= yscale;
                
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
        
        [decodeLock unlock];
    }
}

- (void)drawImageAtIndex:(size_t)index inRect:(CGRect)ir fromRect:(CGRect)fr compositeOperation:(CGCompositeOperation)op context:(CGContextRef)aContext;
{    
    [self drawImageAtIndex:index inRect:ir fromRect:fr adjustedSize:[self size] compositeOperation:op context:aContext];
}

static void drawPattern (void * info, CGContextRef context)
{
    WebImageData *data = (WebImageData *)info;
    
    CGImageRef image = [data imageAtIndex:[data currentFrame]];
    float w = CGImageGetWidth(image);
    float h = CGImageGetHeight(image);
    CGContextDrawImage (context, CGRectMake(0, [data size].height-h, w, h), image);    
}

static const CGPatternCallbacks patternCallbacks = { 0, drawPattern, NULL };

- (void)tileInRect:(CGRect)rect fromPoint:(CGPoint)point context:(CGContextRef)aContext
{
    ASSERT (aContext);

    [decodeLock lock];
    
    size_t frame = [self currentFrame];
    CGImageRef image = [self imageAtIndex:frame];
    if (!image) {
        [decodeLock unlock];
        return;
    }

    if( frame == 0 && isSolidColor ) {
        [self _fillSolidColorInRect: rect compositeOperation: kCGCompositeSover context: aContext];
        
    } else {
        CGSize tileSize = [self size];
        
        // Check and see if a single draw of the image can cover the entire area we are supposed to tile.
        NSRect oneTileRect;
        oneTileRect.origin.x = rect.origin.x + fmodf(fmodf(-point.x, tileSize.width) - tileSize.width, tileSize.width);
        oneTileRect.origin.y = rect.origin.y + fmodf(fmodf(-point.y, tileSize.height) - tileSize.height, tileSize.height);
        oneTileRect.size.height = tileSize.height;
        oneTileRect.size.width = tileSize.width;

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
        CGSize phase = CGSizeMake(fmodf(originInWindow.x, tileSize.width), fmodf(originInWindow.y, tileSize.height));

        // Possible optimization:  We may want to cache the CGPatternRef    
        CGPatternRef pattern = CGPatternCreate(self, CGRectMake (0, 0, tileSize.width, tileSize.height), CGAffineTransformIdentity, tileSize.width, tileSize.height, 
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

    if (isPDF) {
        if (_PDFDoc) {
            CGRect mediaBox = [_PDFDoc mediaBox];
            return mediaBox.size;
        }
    }
    else {
        if (sizeAvailable && !haveSize) {
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

- (float)_floatFileProperty:(CFStringRef)property type:(CFStringRef)type hasProperty:(BOOL *)hasProperty;
{
    [decodeLock lock];
    
    *hasProperty = NO;
    
    CFDictionaryRef properties = [self fileProperties];
    if (!properties) {
        [decodeLock unlock];
        return 0.f;
    }
    
    if (type) {
	properties = CFDictionaryGetValue (properties, type);
	if (!properties) {
	    [decodeLock unlock];
	    return 0.f;
	}
    }
    
    CFNumberRef num = CFDictionaryGetValue (properties, property);
    if (!num) {
        [decodeLock unlock];
        return 0.f;
    }
    
    float value = 0.f;
    CFNumberGetValue (num, kCFNumberFloat32Type, &value);

    [decodeLock unlock];

    *hasProperty = YES;
    
    return value;
}

#define MINIMUM_DURATION (.1)

- (float)_frameDurationAt:(size_t)i
{
    float duration = [self _floatProperty:kCGImagePropertyGIFDelayTime type:kCGImagePropertyGIFDictionary at:i];
    if (duration <= 0.01) {
        /*
            Many annoying ads specify a 0 duration to make an image flash
            as quickly as possible.
            
            We follow mozilla's behavior and set the minimum duration to 
            100 ms.  See 4051389 for more details.
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
    int count;
    BOOL hasProperty;

    // No property means loop once.
    // A property with value 0 means loops forever.
    count = [self _floatFileProperty:kCGImagePropertyGIFLoopCount type:kCGImagePropertyGIFDictionary hasProperty:&hasProperty];
    if (!hasProperty)
	count = -1;
	
    return count;
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
    while ((animation = [objectEnumerator nextObject])) {
	NSSet *renderersInView = (NSSet *)CFDictionaryGetValue (animation->animatingRenderers, aView);
        if (renderersInView) {
			if (!renderersToStop)
				renderersToStop = [[NSMutableSet alloc] init];
            [renderersToStop unionSet:renderersInView];
        }
    }

    // Now tell them all to stop drawing.
    [renderersToStop makeObjectsPerformSelector:@selector(stopAnimation)];
	[renderersToStop release];
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

- (void)resetAnimation
{
    [self _stopAnimation];
    currentFrame = 0;
    repetitionsComplete = 0;
    animationFinished = NO;
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
	    currentFrame--;
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

-(void)_createPDFWithData:(NSData *)data
{
    if (!_PDFDoc) {
        _PDFDoc = [[WebPDFDocument alloc] initWithData:data];
    }
}

- (CGPDFDocumentRef)_PDFDocumentRef
{
    return [_PDFDoc documentRef];
}

- (void)_PDFDrawInContext:(CGContextRef)context
{
    CGPDFDocumentRef document = [self _PDFDocumentRef];
    if (document != NULL) {
        CGRect       mediaBox = [_PDFDoc mediaBox];
        
        CGContextSaveGState(context);
        // Rotate translate image into position according to doc properties.
        [_PDFDoc adjustCTM:context];	

        // Media box may have non-zero origin which we ignore. CGPDFDocumentShowPage pages start
        // at 1, not 0.
        CGContextDrawPDFDocument(context, CGRectMake(0, 0, mediaBox.size.width, mediaBox.size.height), document, 1);

        CGContextRestoreGState(context);
    }
}

- (BOOL)_PDFDrawFromRect:(NSRect)srcRect toRect:(NSRect)dstRect operation:(CGCompositeOperation)op alpha:(float)alpha flipped:(BOOL)flipped context:(CGContextRef)context
{
    float hScale, vScale;

    CGContextSaveGState(context);

    CGContextSetCompositeOperation (context, op);

    // Scale and translate so the document is rendered in the correct location.
    hScale = dstRect.size.width  / srcRect.size.width;
    vScale = dstRect.size.height / srcRect.size.height;

    CGContextTranslateCTM (context, dstRect.origin.x - srcRect.origin.x * hScale, dstRect.origin.y - srcRect.origin.y * vScale);
    CGContextScaleCTM (context, hScale, vScale);

    // Reverse if flipped image.
    if (flipped) {
        CGContextScaleCTM(context, 1, -1);
        CGContextTranslateCTM (context, 0, -dstRect.size.height);
    }

    // Clip to destination in case we are imaging part of the source only
    CGContextClipToRect(context, CGRectIntegral(*(CGRect*)&srcRect));

    // and draw
    [self _PDFDrawInContext:context];

    // done with our fancy transforms
    CGContextRestoreGState(context);

    return YES;
}

@end

#endif
