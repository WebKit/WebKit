/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import <WebKit/WebImageRenderer.h>

#import <WebKit/WebAssertions.h>
#import <WebKit/WebImageRendererFactory.h>
#import <WebKit/WebGraphicsBridge.h>
#import <WebKit/WebHTMLView.h>
#import <WebKit/WebImageView.h>
#import <WebKit/WebNSObjectExtras.h>
#import <WebKitSystemInterface.h>

#import <WebCore/WebCoreImageRenderer.h>

#ifdef USE_CGIMAGEREF

#import <WebKit/WebImageData.h>

// Forward declarations of internal methods.
@interface WebImageRenderer (WebInternal)
- (void)_startOrContinueAnimationIfNecessary;
@end

@implementation WebImageRenderer

- (id)initWithMIMEType:(NSString *)MIME
{
    self = [super init];
    if (self != nil) {
        MIMEType = [MIME copy];
    }
    return self;
}

- (id)initWithData:(NSData *)data MIMEType:(NSString *)MIME
{
    self = [super init];
    if (self != nil) {
        MIMEType = [MIME copy];
        imageData = [[WebImageData alloc] init];
        [imageData incrementalLoadWithBytes:[data bytes] length:[data length] complete:YES callback:0];
    }
    return self;
}

- (id)initWithContentsOfFile:(NSString *)filename
{
    self = [super init];

    NSBundle *bundle = [NSBundle bundleForClass:[self class]];
    NSString *imagePath = [bundle pathForResource:filename ofType:@"tiff"];

    imageData = [[WebImageData alloc] init];
    NSData *data = [NSData dataWithContentsOfFile:imagePath];
    [imageData incrementalLoadWithBytes:[data bytes] length:[data length] complete:YES callback:0];
        

    return self;
}

- (void)dealloc
{
    [MIMEType release];
    [imageData release];
    [nsimage release];
    [TIFFData release];
    [super dealloc];
}

- copyWithZone:(NSZone *)zone
{
    WebImageRenderer *copy;

    copy = [[WebImageRenderer alloc] init];
    copy->MIMEType = [MIMEType copy];
    copy->adjustedSize = adjustedSize;
    copy->isSizeAdjusted = isSizeAdjusted;
    copy->imageData = [imageData retain];
        
    return copy;
}

- (id <WebCoreImageRenderer>)retainOrCopyIfNeeded
{
    return [self copyWithZone:0];
}

- (void)resize:(NSSize)s
{
    isSizeAdjusted = YES;
    adjustedSize = s;
}

- (NSSize)size
{
    if (isSizeAdjusted)
        return adjustedSize;
        
    if (!imageData)
        return NSZeroSize;
	
    CGSize sz = [imageData size];
    return NSMakeSize(sz.width, sz.height);
}

- (NSString *)MIMEType
{
    return MIMEType;
}

- (int)frameCount
{
    return [imageData numberOfImages];
}


- (BOOL)isNull
{
    return [imageData isNull];
}

- (BOOL)incrementalLoadWithBytes:(const void *)bytes length:(unsigned)length complete:(BOOL)isComplete callback:(id)c
{
    if (!imageData) {
        imageData = [[WebImageData alloc] init];
        if ([MIMEType isEqual:@"application/pdf"]) {
            [imageData setIsPDF:YES];
        }
    }
    return [imageData incrementalLoadWithBytes:bytes length:length complete:isComplete callback:c];
}

- (void)drawImageInRect:(NSRect)ir fromRect:(NSRect)fr
{
    CGContextRef aContext = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];
    NSCompositingOperation op = NSCompositeSourceOver;

    [self drawImageInRect:ir fromRect:fr compositeOperator:op context:aContext];
}

- (void)drawImageInRect:(NSRect)ir fromRect:(NSRect)fr compositeOperator:(NSCompositingOperation)operator context:(CGContextRef)aContext
{
    if (aContext == 0)
        aContext = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];
    
    NSCompositingOperation op = (NSCompositingOperation)operator;
        
    if (isSizeAdjusted) {
        [imageData drawImageAtIndex:[imageData currentFrame] inRect:CGRectMake(ir.origin.x, ir.origin.y, ir.size.width, ir.size.height) 
                fromRect:CGRectMake(fr.origin.x, fr.origin.y, fr.size.width, fr.size.height) 
                adjustedSize:CGSizeMake(adjustedSize.width, adjustedSize.height)
                compositeOperation:op context:aContext];
    }
    else {
        [imageData drawImageAtIndex:[imageData currentFrame] inRect:CGRectMake(ir.origin.x, ir.origin.y, ir.size.width, ir.size.height) 
                fromRect:CGRectMake(fr.origin.x, fr.origin.y, fr.size.width, fr.size.height) 
                compositeOperation:op context:aContext];
    }

    targetAnimationRect = ir;
    [self _startOrContinueAnimationIfNecessary];
}

- (void)tileInRect:(NSRect)rect fromPoint:(NSPoint)point context:(CGContextRef)aContext
{
    if (aContext == 0)
        aContext = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];

    [imageData tileInRect:CGRectMake(rect.origin.x, rect.origin.y, rect.size.width, rect.size.height)
            fromPoint:CGPointMake(point.x, point.y) context:aContext];
            
    targetAnimationRect = rect;
    [self _startOrContinueAnimationIfNecessary];
}

- (void)_startOrContinueAnimationIfNecessary
{
    NSView *targetView = [NSView focusView];
    
    // Only animate if we're drawing into a WebHTMLView or WebImageView.  This fixes problems
    // like <rdar://problem/3966973>, which describes a third party application that renders thumbnails of
    // the page into a alternate view.
    if (([targetView isKindOfClass:[WebHTMLView class]] || [targetView isKindOfClass:[WebImageView class]]) 
	    && [imageData shouldAnimate] && [MIMEType isEqual:@"image/gif"]) {
        [imageData addAnimatingRenderer:self inView:targetView];
        [imageData animate];
    }
}

+ (void)stopAnimationsInView:(NSView *)aView
{
    [WebImageData stopAnimationsInView:aView];
}

- (void)resetAnimation
{
    [imageData resetAnimation];
}


- (void)stopAnimation
{
    [imageData removeAnimatingRenderer:self];
}

- (NSRect)targetAnimationRect
{
    return targetAnimationRect;
}

- (void)increaseUseCount
{
}

- (void)decreaseUseCount
{
}

- (void)flushRasterCache
{
}

- (CGImageRef)imageRef
{
    return [imageData imageAtIndex:0];
}

- (NSData *)TIFFRepresentation
{
    if (!TIFFData) {
        CGImageRef image = [imageData imageAtIndex:0];
        if (!image)
            return 0;
            
        CFMutableDataRef data = 0;
        CGImageDestinationRef destination = 0;
        
        data = CFDataCreateMutable(NULL, 0);
        // FIXME:  Use type kCGImageTypeIdentifierTIFF constant once is becomes available in the API
        destination = CGImageDestinationCreateWithData (data, CFSTR("public.tiff"), 1, NULL);
        if (destination) {
            CGImageDestinationAddImage (destination, image, NULL);
            if (!CGImageDestinationFinalize (destination)) {
                ERROR ("Unable to create image\n");
            }
            CFRelease (destination);
        }

        TIFFData = (NSData *)data;
    }
    
    return TIFFData;
}

- (NSImage *)image
{
    if (!nsimage)
        nsimage = [[NSImage alloc] initWithData:[self TIFFRepresentation]];
    return nsimage;
}

@end

#else

@interface WebInternalImage : NSImage <NSCopying>
{
    NSTimer *frameTimer;
    NSView *frameView;
    NSRect imageRect;
    NSRect targetRect;

    int loadStatus;

    NSColor *patternColor;
    int patternColorLoadStatus;

    int repetitionsComplete;
    BOOL animationFinished;
    
    NSPoint tilePoint;
    BOOL animatedTile;

    int compositeOperator;
    id redirectContext;
    CGContextRef context;
    BOOL needFlushRasterCache;
    BOOL rasterFlushing;
    NSImageCacheMode rasterFlushingOldMode;
    
    NSString *MIMEType;
    BOOL isNull;
    int useCount;

    CGImageRef cachedImageRef;
    
    id _PDFDoc;
        
@public    
    NSData *originalData;
}

- (id)initWithMIMEType:(NSString *)MIME;
- (id)initWithData:(NSData *)data MIMEType:(NSString *)MIME;

- (void)releasePatternColor;

- (NSString *)MIMEType;
- (int)frameCount;

- (BOOL)incrementalLoadWithBytes:(const void *)bytes length:(unsigned)length complete:(BOOL)isComplete callback:(id)c;
- (void)resize:(NSSize)s;
- (void)drawImageInRect:(NSRect)ir fromRect:(NSRect)fr;
- (void)drawImageInRect:(NSRect)ir fromRect:(NSRect)fr compositeOperator:(NSCompositingOperation)compsiteOperator context:(CGContextRef)context;
- (void)stopAnimation;
- (void)tileInRect:(NSRect)rect fromPoint:(NSPoint)point context:(CGContextRef)aContext;
- (BOOL)isNull;
- (void)increaseUseCount;
- (void)decreaseUseCount;
- (WebImageRenderer *)createRendererIfNeeded;
- (void)flushRasterCache;
- (CGImageRef)imageRef;

+ (void)stopAnimationsInView:(NSView *)aView;
- (void)resetAnimation;

- (void)startAnimationIfNecessary;
- (NSGraphicsContext *)_beginRedirectContext:(CGContextRef)aContext;
- (void)_endRedirectContext:(NSGraphicsContext *)aContext;
- (void)_needsRasterFlush;
- (BOOL)_PDFDrawFromRect:(NSRect)srcRect toRect:(NSRect)dstRect operation:(NSCompositingOperation)op alpha:(float)alpha flipped:(BOOL)flipped;

@end

extern NSString *NSImageLoopCount;

/*
    We need to get the AppKit to redirect drawing to our
    CGContext.  There is currently (on Panther) no public
    way to make this happen.  So we create a NSCGSContext
    subclass and twiddle it's _cgsContext ivar directly.
    Very fragile, but the only way...
*/
@interface NSCGSContext : NSGraphicsContext {
    CGContextRef _cgsContext;
}
@end

static CGImageRef _createImageRef(NSBitmapImageRep *rep);

@interface NSBitmapImageRep (AppKitInternals)
- (CGImageRef)_CGImageRef;
@end

@interface NSFocusStack : NSObject
@end

@interface WebImageContext : NSCGSContext {
  @private
    NSFocusStack* _focusStack;
    NSRect        _bounds;
    BOOL          _isFlipped;
}
- (id)initWithBounds:(NSRect)b context:(CGContextRef)context;
- (NSRect)bounds;
@end


@implementation WebImageContext

- (id)initWithBounds:(NSRect)b context:(CGContextRef)context {
    
    self = [super init];
    if (self != nil) {
	_bounds     = b;
	_isFlipped  = YES;
        if (context) {
            _cgsContext = CGContextRetain(context);
        }
    }
    
    return self;
}

- (void)dealloc
{
    [_focusStack release];
    if (_cgsContext) {
        CGContextRelease(_cgsContext);
        _cgsContext = 0; // super dealloc may also release
    }
    [super dealloc];
}

- (void)finalize
{
    if (_cgsContext) {
        CGContextRelease(_cgsContext);
        _cgsContext = 0; // super finalize may also release
    }
    [super finalize];
}

- (void)saveGraphicsState
{
    if (_cgsContext) {
        CGContextSaveGState(_cgsContext);
    }
}

- (void)restoreGraphicsState
{
    if (_cgsContext) {
        CGContextRestoreGState(_cgsContext);
    }
}

- (BOOL)isDrawingToScreen
{
    return NO;
}

- (void *)focusStack
{
    if (!_focusStack) _focusStack = [[NSFocusStack allocWithZone:NULL] init];
    return _focusStack;
}

- (void)setFocusStack:(void *)stack
{
    id oldstack = _focusStack;
    _focusStack = [(id)stack retain];
    [oldstack release];
}

- (NSRect)bounds
{
    return _bounds;
}

- (BOOL)isFlipped
{
    return _isFlipped;
}

@end

#define MINIMUM_DURATION (1.0/30.0)

@implementation WebInternalImage

static NSMutableSet *activeImageRenderers;

+ (void)stopAnimationsInView:(NSView *)aView
{
    NSEnumerator *objectEnumerator = [activeImageRenderers objectEnumerator];
    WebInternalImage *renderer;
    NSMutableSet *renderersToStop = [[NSMutableSet alloc] init];

    while ((renderer = [objectEnumerator nextObject])) {
        if (renderer->frameView == aView) {
            [renderersToStop addObject: renderer];
        }
    }

    objectEnumerator = [renderersToStop objectEnumerator];
    while ((renderer = [objectEnumerator nextObject])) {
        if (renderer->frameView == aView) {
            [renderer stopAnimation];
        }
    }
    [renderersToStop release];
}

- (id)initWithMIMEType:(NSString *)MIME
{
    self = [super init];
    if (self != nil) {
        // Work around issue with flipped images and TIFF by never using the image cache.
        // See bug 3344259 and related bugs.
        [self setCacheMode:NSImageCacheNever];

        loadStatus = NSImageRepLoadStatusUnknownType;
        MIMEType = [MIME copy];
        isNull = YES;
        compositeOperator = (int)NSCompositeSourceOver;
    }
    
    return self;
}

- (id)initWithData:(NSData *)data MIMEType:(NSString *)MIME
{
    WebInternalImage *result = nil;

    NS_DURING
    
        result = [super initWithData:data];
        if (result != nil) {
            // Work around issue with flipped images and TIFF by never using the image cache.
            // See bug 3344259 and related bugs.
            [result setCacheMode:NSImageCacheNever];
    
            result->loadStatus = NSImageRepLoadStatusUnknownType;
            result->MIMEType = [MIME copy];
            result->isNull = [data length] == 0;
            result->compositeOperator = (int)NSCompositeSourceOver;
        }

    NS_HANDLER

        result = nil;

    NS_ENDHANDLER

    return result;
}

- (id)initWithContentsOfFile:(NSString *)filename
{
    NSBundle *bundle = [NSBundle bundleForClass:[self class]];
    NSString *imagePath = [bundle pathForResource:filename ofType:@"tiff"];
    WebInternalImage *result = nil;

    NS_DURING

        result = [super initWithContentsOfFile:imagePath];
        if (result != nil) {
            // Work around issue with flipped images and TIFF by never using the image cache.
            // See bug 3344259 and related bugs.
            [result setCacheMode:NSImageCacheNever];
    
            result->loadStatus = NSImageRepLoadStatusUnknownType;
            result->compositeOperator = (int)NSCompositeSourceOver;
        }
        
    NS_HANDLER

        result = nil;

    NS_ENDHANDLER

    return result;
}

- (void)increaseUseCount
{
    useCount++;
}

- (void)decreaseUseCount
{
    useCount--;
}

- (WebImageRenderer *)createRendererIfNeeded
{
    // If an animated image appears multiple times in a given page, we
    // must create multiple WebCoreImageRenderers so that each copy
    // animates. However, we don't want to incur the expense of
    // re-decoding for the very first use on a page, since QPixmap
    // assignment always calls this method, even when just fetching
    // the image from the cache for the first time for a page.
    if (originalData && useCount) {
        return [[[WebImageRendererFactory sharedFactory] imageRendererWithData:originalData MIMEType:MIMEType] retain];
    }
    return nil;
}

- copyWithZone:(NSZone *)zone
{
    // FIXME: If we copy while doing an incremental load, it won't work.
    WebInternalImage *copy;

    copy = [super copyWithZone:zone];
    copy->MIMEType = [MIMEType copy];
    copy->originalData = [originalData retain];
    copy->frameTimer = nil;
    copy->frameView = nil;
    copy->patternColor = nil;
    copy->compositeOperator = compositeOperator;
    copy->context = 0;

    return copy;
}

- (BOOL)isNull
{
    return isNull;
}

- (void)_adjustSizeToPixelDimensions
{
    // Force the image to use the pixel size and ignore the dpi.
    // Ignore any absolute size in the image and always use pixel dimensions.
    NSBitmapImageRep *imageRep = [[self representations] objectAtIndex:0];
    NSSize size = NSMakeSize([imageRep pixelsWide], [imageRep pixelsHigh]);
    [imageRep setSize:size];
        
    [self setScalesWhenResized:YES];
    [self setSize:size];
}

- (BOOL)incrementalLoadWithBytes:(const void *)bytes length:(unsigned)length complete:(BOOL)isComplete callback:(id)c
{
    NSArray *reps = [self representations];
    NSBitmapImageRep *imageRep = [reps count] > 0 ? [[self representations] objectAtIndex:0] : nil;
    
    if (imageRep && [imageRep isKindOfClass: [NSBitmapImageRep class]]) {
        NSData *data = [[NSData alloc] initWithBytes:bytes length:length];

        NS_DURING
            // Get rep again to avoid bogus compiler warning.
            NSBitmapImageRep *aRep = [[self representations] objectAtIndex:0];

            loadStatus = [aRep incrementalLoadFromData:data complete:isComplete];
        NS_HANDLER
            loadStatus = NSImageRepLoadStatusInvalidData; // Arbitrary choice; any error will do.
        NS_ENDHANDLER

        // Hold onto the original data in case we need to copy this image.  (Workaround for appkit NSImage
        // copy flaw).
        if (isComplete && [self frameCount] > 1)
            originalData = data;
        else
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
            //printf ("NSImageRepLoadStatusCompleted size %d, isComplete %d\n", length, isComplete);
            [self _adjustSizeToPixelDimensions];        
            isNull = NO;
            return YES;
        default:
            [self _adjustSizeToPixelDimensions];
            //printf ("incrementalLoadWithBytes: size %d, isComplete %d\n", length, isComplete);
            // We have some valid data.  Return YES so we can attempt to draw what we've got.
            isNull = NO;
            return YES;
        }
    }
    else {
        if (isComplete) {
            originalData = [[NSData alloc] initWithBytes:bytes length:length];
            if ([MIMEType isEqual:@"application/pdf"]) {
                Class repClass = [NSImageRep imageRepClassForData:originalData];
                if (repClass) {
                    NSImageRep *rep = [[repClass alloc] initWithData:originalData];
                    [self addRepresentation:rep];
                }
            }
            isNull = NO;
            return YES;
        }
    }
    return NO;
}

- (void)dealloc
{
    ASSERT(frameTimer == nil);
    ASSERT(frameView == nil);
    [patternColor release];
    [MIMEType release];
    [originalData release];
    
    if (context) {
        CGContextRelease(context);
        context = 0;
    }

    if (cachedImageRef) {
        CGImageRelease (cachedImageRef);
        cachedImageRef = 0;
    }
    
    [_PDFDoc release];

    [super dealloc];
}

- (void)finalize
{
    ASSERT(frameTimer == nil);
    ASSERT(frameView == nil);

    if (context) {
        CGContextRelease(context);
    }

    if (cachedImageRef) {
        CGImageRelease (cachedImageRef);
        cachedImageRef = 0;
    }
    
    [super finalize];
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
    return count;
}

- (void)scheduleFrame
{
    if (frameTimer && [frameTimer isValid])
        return;
    frameTimer = [[NSTimer scheduledTimerWithTimeInterval:[self frameDuration]
                                                    target:self
                                                  selector:@selector(nextFrame:)
                                                  userInfo:nil
                                                   repeats:NO] retain];
}

- (NSGraphicsContext *)_beginRedirectContext:(CGContextRef)aContext
{
    NSGraphicsContext *oldContext = 0;
    if (aContext) {
        oldContext = [NSGraphicsContext currentContext];
        // Assumes that we are redirecting to a CGBitmapContext.
        size_t w = CGBitmapContextGetWidth (aContext);
        size_t h = CGBitmapContextGetHeight (aContext);
        redirectContext = [[WebImageContext alloc] initWithBounds:NSMakeRect(0, 0, (float)w, (float)h) context:aContext];
        [NSGraphicsContext setCurrentContext:redirectContext];
    }
    return oldContext; 
}

- (void)_endRedirectContext:(NSGraphicsContext *)aContext
{
    if (aContext) {
        [NSGraphicsContext setCurrentContext:aContext];
        [redirectContext autorelease];
        redirectContext = 0;
    }
}

- (void)_needsRasterFlush
{
#if 0
    if (needFlushRasterCache && [MIMEType isEqual: @"application/pdf"]) {
        // FIXME:  At this point we need to flush the cached rasterized PDF.
    }
#endif
}

- (void)_adjustColorSpace
{
#if COLORMATCH_EVERYTHING
    NSArray *reps = [self representations];
    NSBitmapImageRep *imageRep = [reps count] > 0 ? [[self representations] objectAtIndex:0] : nil;
    if (imageRep && [imageRep isKindOfClass: [NSBitmapImageRep class]] &&
        [imageRep valueForProperty:NSImageColorSyncProfileData] == nil &&
        [[imageRep colorSpaceName] isEqualToString:NSDeviceRGBColorSpace]) {
        [imageRep setColorSpaceName:NSCalibratedRGBColorSpace];
    }
#else
    NSArray *reps = [self representations];
    NSBitmapImageRep *imageRep = [reps count] > 0 ? [[self representations] objectAtIndex:0] : nil;
    if (imageRep && [imageRep isKindOfClass: [NSBitmapImageRep class]] &&
        [imageRep valueForProperty:NSImageColorSyncProfileData] == nil &&
        [[imageRep colorSpaceName] isEqualToString:NSCalibratedRGBColorSpace]) {
        [imageRep setColorSpaceName:NSDeviceRGBColorSpace];
    }
#endif
}


- (void)drawClippedToValidInRect:(NSRect)ir fromRect:(NSRect)fr
{
    [self _adjustColorSpace];

    if (loadStatus >= 0) {
        // The last line might be a partial line, so the number of complete lines is the number
        // we get from NSImage minus one.
        int numCompleteLines = loadStatus - 1;
        if (numCompleteLines <= 0) {
            return;
        }
        int pixelsHigh = [[[self representations] objectAtIndex:0] pixelsHigh];
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
            ASSERT([self isFlipped]);
            ASSERT([[NSView focusView] isFlipped]);
            ir.size.height = clippedDestinationHeight;
            fr.size.height = clippedSourceHeight;
        }
    }
    
    if (context) {
        NSGraphicsContext *oldContext = [self _beginRedirectContext:context];
        [self _needsRasterFlush];

        // If we have PDF then draw the PDF ourselves, bypassing the NSImage caching mechanisms,
        // but only do this when we're rendering to an offscreen context.  NSImage will always
        // cache the PDF image at it's native resolution, thus, causing artifacts when the image
        // is drawn into a scaled or rotated context.
        if ([MIMEType isEqual:@"application/pdf"])
            [self _PDFDrawFromRect:fr toRect:ir operation:compositeOperator alpha:1.0 flipped:YES];
        else
            [self drawInRect:ir fromRect:fr operation:compositeOperator fraction: 1.0];

        [self _endRedirectContext:oldContext];
    }
    else {
        [self drawInRect:ir fromRect:fr operation:compositeOperator fraction: 1.0];
    }
}

- (CGPDFDocumentRef)_PDFDocumentRef
{
    if (!_PDFDoc) {
        _PDFDoc = [[WebPDFDocument alloc] initWithData:originalData];
    }
        
    return [_PDFDoc documentRef];
}

- (void)_PDFDraw
{
    CGPDFDocumentRef document = [self _PDFDocumentRef];
    if (document != NULL) {
        CGContextRef _context  = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];
        CGRect       mediaBox = [_PDFDoc mediaBox];
        
        CGContextSaveGState(_context);
        // Rotate translate image into position according to doc properties.
        [_PDFDoc adjustCTM:_context];	

        // Media box may have non-zero origin which we ignore. CGPDFDocumentShowPage pages start
        // at 1, not 0.
        CGContextDrawPDFDocument(_context, CGRectMake(0, 0, mediaBox.size.width, mediaBox.size.height), document, 1);

        CGContextRestoreGState(_context);
    }
}

- (BOOL)_PDFDrawFromRect:(NSRect)srcRect toRect:(NSRect)dstRect operation:(NSCompositingOperation)op alpha:(float)alpha flipped:(BOOL)flipped
{
    // FIXME:  The rasterized PDF should be drawn into a cache, and the raster then composited.
    
    CGContextRef _context = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];
    float hScale, vScale;

    CGContextSaveGState(_context);

    [[NSGraphicsContext currentContext] setCompositingOperation:op];
    // Scale and translate so the document is rendered in the correct location.
    hScale = dstRect.size.width  / srcRect.size.width;
    vScale = dstRect.size.height / srcRect.size.height;

    CGContextTranslateCTM (_context, dstRect.origin.x - srcRect.origin.x * hScale, dstRect.origin.y - srcRect.origin.y * vScale);
    CGContextScaleCTM (_context, hScale, vScale);

    // Reverse if flipped image.
    if (flipped) {
        CGContextScaleCTM(_context, 1, -1);
        CGContextTranslateCTM (_context, 0, -(dstRect.origin.y + dstRect.size.height));
    }

    // Clip to destination in case we are imaging part of the source only
    CGContextClipToRect(_context, CGRectIntegral(*(CGRect*)&srcRect));

    // and draw
    [self _PDFDraw];

    // done with our fancy transforms
    CGContextRestoreGState(_context);

    return YES;
}

- (void)resetAnimation
{
    [self stopAnimation];
    [self setCurrentFrame:0];
    repetitionsComplete = 0;
    animationFinished = NO;
}

- (void)nextFrame:(id)context
{
    int currentFrame;
    
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
    
    // Release the tiling pattern so next frame will update the pattern if we're tiling.
    [patternColor release];
    patternColor = nil;
    
    [frameView setNeedsDisplayInRect:targetRect];
}

// Will be called when the containing view is displayed by WebCore RenderImage (via QPainter).
// If the image is an animated image it will begin animating.  If the image is already animating,
// it's frame will have been advanced by nextFrame:.
//
// Also used to draw the image by WebImageView.
- (void)drawImageInRect:(NSRect)ir fromRect:(NSRect)fr
{
    if (animatedTile){
        [self tileInRect:ir fromPoint:tilePoint context:context];
    }
    else {
        [self drawClippedToValidInRect:ir fromRect:fr];
        imageRect = fr;
        targetRect = ir;
        [self startAnimationIfNecessary];
    }
}

- (void)flushRasterCache
{
    needFlushRasterCache = YES;
}

- (void)drawImageInRect:(NSRect)ir fromRect:(NSRect)fr compositeOperator:(NSCompositingOperation)operator context:(CGContextRef)aContext
{
    compositeOperator = operator;
    
    if (aContext != context) {
        if (aContext) {
            CGContextRetain(aContext);
        }
        if (context) {
            CGContextRelease(context);
        }
        context = aContext;
    }
        
    [self drawImageInRect:ir fromRect:fr];
}

- (void)startAnimationIfNecessary
{
    if ([self frameCount] > 1 && !animationFinished) {
        NSView *newView = [NSView focusView];
        if (newView != frameView){
            [frameView release];
            frameView = [newView retain];
        }
        [self scheduleFrame];
        if (!activeImageRenderers) {
            activeImageRenderers = [[NSMutableSet alloc] init];
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

- (void)tileInRect:(NSRect)rect fromPoint:(NSPoint)point context:(CGContextRef)aContext
{
    // These calculations are only correct for the flipped case.
    ASSERT([self isFlipped]);
    ASSERT([[NSView focusView] isFlipped]);

    [self _adjustColorSpace];

    NSSize size = [self size];

    // Check and see if a single draw of the image can cover the entire area we are supposed to tile.
    NSRect oneTileRect;
    oneTileRect.origin.x = rect.origin.x + fmodf(fmodf(-point.x, size.width) - size.width, size.width);
    oneTileRect.origin.y = rect.origin.y + fmodf(fmodf(-point.y, size.height) - size.height, size.height);
// I think this is a simpler way to say the same thing.  Also, if either point.x or point.y is negative, both
// methods would end up with the wrong answer.  For example, fmod(-22,5) is -2, which is the correct delta to
// the start of the pattern, but fmod(-(-23), 5) is 3.  This is the delta to the *end* of the pattern
// instead of the start, so onTileRect will be too far right.
//    oneTileRect.origin.x = rect.origin.x - fmodf(point.x, size.width);
//    oneTileRect.origin.y = rect.origin.y - fmodf(point.y, size.height);
    oneTileRect.size = size;

    // Compute the appropriate phase relative to the top level view in the window.
    // Conveniently, the oneTileRect we computed above has the appropriate origin.
    NSPoint originInWindow = [[NSView focusView] convertPoint:oneTileRect.origin toView:nil];
    // WebCore may translate the focus, and thus need an extra phase correction
    NSPoint extraPhase = [[WebGraphicsBridge sharedBridge] additionalPatternPhase];
    originInWindow.x += extraPhase.x;
    originInWindow.y += extraPhase.y;
    CGSize phase = CGSizeMake(fmodf(originInWindow.x, size.width), fmodf(originInWindow.y, size.height));
    
    // If the single image draw covers the whole area, then just draw once.
    if (NSContainsRect(oneTileRect, rect)) {
        NSRect fromRect;

        fromRect.origin.x = rect.origin.x - oneTileRect.origin.x;
        fromRect.origin.y = rect.origin.y - oneTileRect.origin.y;
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
        
    NSGraphicsContext *oldContext = [self _beginRedirectContext:context];
    [self _needsRasterFlush];
    
    [NSGraphicsContext saveGraphicsState];
    
    CGContextSetPatternPhase((CGContextRef)[[NSGraphicsContext currentContext] graphicsPort], phase);    
    [patternColor set];
    [NSBezierPath fillRect:rect];
    
    [NSGraphicsContext restoreGraphicsState];

    [self _endRedirectContext:oldContext];

    animatedTile = YES;
    tilePoint = point;
    targetRect = rect;
    [self startAnimationIfNecessary];
}

- (void)resize:(NSSize)s
{
    [self setScalesWhenResized:YES];
    [self setSize:s];
}

- (NSString *)MIMEType
{
    return MIMEType;
}

// Try hard to get a CGImageRef.  First try to snag one from the
// NSBitmapImageRep, then try to create one.  In some cases we may
// not be able to create one.
- (CGImageRef)imageRef
{
    CGImageRef ref = 0;
    
    if (cachedImageRef)
        return cachedImageRef;
        
    if ([[self representations] count] > 0) {
        NSBitmapImageRep *rep = [[self representations] objectAtIndex:0];
        
        if ([rep respondsToSelector:@selector(_CGImageRef)])
            ref =  [rep _CGImageRef];
            
        if (!ref) {
            cachedImageRef = _createImageRef(rep);
            ref = cachedImageRef;
        }
    }
    return ref;
}

- (void)releasePatternColor
{
    [patternColor release];
    patternColor = nil;
}

@end

@implementation WebImageRenderer

- (id)initWithMIMEType:(NSString *)MIME
{
    WebInternalImage *i = [[WebInternalImage alloc] initWithMIMEType:MIME];
    if (i == nil) {
        [self dealloc];
        return nil;
    }
    [self init];
    image = i;
    return self;
}

- (id)initWithData:(NSData *)data MIMEType:(NSString *)MIME
{
    WebInternalImage *i = [[WebInternalImage alloc] initWithData:data MIMEType:MIME];
    if (i == nil) {
        [self dealloc];
        return nil;
    }
    [self init];
    image = i;
    return self;
}

- (id)initWithContentsOfFile:(NSString *)filename
{
    WebInternalImage *i = [[WebInternalImage alloc] initWithContentsOfFile:filename];
    if (i == nil) {
        [self dealloc];
        return nil;
    }
    [self init];
    image = i;
    return self;
}

- (void)dealloc
{
    [image releasePatternColor];
    [image release];
    [super dealloc];
}

- (NSImage *)image
{
    return image;
}

- (NSString *)MIMEType
{
    return [image MIMEType];
}

- (NSData *)TIFFRepresentation
{
    return [image TIFFRepresentation];
}

- (int)frameCount
{
    return [image frameCount];
}

- (void)setOriginalData:(NSData *)data
{
    NSData *oldData = image->originalData;
    image->originalData = [data retain];
    [oldData release];
}

+ (void)stopAnimationsInView:(NSView *)aView
{
    [WebInternalImage stopAnimationsInView:aView];
}

- (BOOL)incrementalLoadWithBytes:(const void *)bytes length:(unsigned)length complete:(BOOL)isComplete callback:(id)c
{
    return [image incrementalLoadWithBytes:bytes length:length complete:isComplete callback:c];
}

- (NSSize)size
{
    if (!image)
	return NSZeroSize;
	
    return [image size];
}

- (void)resize:(NSSize)s
{
    [image resize:s];
}

- (void)drawImageInRect:(NSRect)ir fromRect:(NSRect)fr
{
    [image drawImageInRect:ir fromRect:fr];
}

- (void)drawImageInRect:(NSRect)ir fromRect:(NSRect)fr compositeOperator:(NSCompositingOperation)compsiteOperator context:(CGContextRef)context
{
    [image drawImageInRect:ir fromRect:fr compositeOperator:compsiteOperator context:context];
}

- (void)resetAnimation
{
    [image resetAnimation];
}

- (void)stopAnimation
{
    [image stopAnimation];
}

- (void)tileInRect:(NSRect)r fromPoint:(NSPoint)p context:(CGContextRef)context
{
    [image tileInRect:r fromPoint:p context:context];
}

- (BOOL)isNull
{
    return image == nil || [image isNull];
}

- (id <WebCoreImageRenderer>)retainOrCopyIfNeeded
{
    WebImageRenderer *newRenderer = [image createRendererIfNeeded];
    if (newRenderer) {
        return newRenderer;
    }
    [self retain];
    return self;
}

- (void)increaseUseCount
{
    [image increaseUseCount];
}

- (void)decreaseUseCount
{
    [image decreaseUseCount];
}

- (void)flushRasterCache
{
    [image flushRasterCache];
}

- (CGImageRef)imageRef
{
    return [image imageRef];
}

- (id)copyWithZone:(NSZone *)zone
{
    WebImageRenderer *copy = [[WebImageRenderer alloc] init];
    copy->image = [image copy];
    return copy;
}

@end

static CGImageRef _createImageRef(NSBitmapImageRep *rep) {
    BOOL isPlanar = [rep isPlanar];
    if (isPlanar)
        return 0;
        
    const unsigned char *bitmapData = [rep bitmapData];
    int pixelsWide = [rep pixelsWide];
    int pixelsHigh = [rep pixelsHigh];
    int bitsPerSample = [rep bitsPerSample];
    int bitsPerPixel = [rep bitsPerPixel];
    int bytesPerRow = [rep bytesPerRow];
    BOOL hasAlpha = [rep hasAlpha];
    CGImageRef image;
    CGDataProviderRef dataProvider;

    CGColorSpaceRef colorSpace = WebCGColorSpaceCreateRGB();
    dataProvider = CGDataProviderCreateWithData(NULL, bitmapData, pixelsHigh * bytesPerRow, NULL);

    image = CGImageCreate(pixelsWide, pixelsHigh, bitsPerSample, bitsPerPixel, bytesPerRow, colorSpace,
                          hasAlpha ? kCGImageAlphaPremultipliedLast : kCGImageAlphaNone,
                          dataProvider, NULL, false /*shouldInterpolate*/, kCGRenderingIntentDefault);

    CGDataProviderRelease(dataProvider);
    CGColorSpaceRelease(colorSpace);	

    return image;
}

#endif

//------------------------------------------------------------------------------------

@implementation WebPDFDocument

static void ReleasePDFDocumentData(void *info, const void *data, size_t size) {
    [(NSData*)info autorelease];
}

- (id) initWithData:(NSData*)data
{
    self = [super init];
    if (self != nil)
    {
        if (data != nil) {
            CGDataProviderRef dataProvider = CGDataProviderCreateWithData([data retain], [data bytes], [data length], ReleasePDFDocumentData);
            _document = CGPDFDocumentCreateWithProvider(dataProvider);
            CGDataProviderRelease(dataProvider);
        }
        _currentPage = -1;
        [self setCurrentPage:0];
    }
    return self;
}

- (void)dealloc
{
    if (_document != NULL) CGPDFDocumentRelease(_document);
    [super dealloc];
}

- (void)finalize
{
    if (_document != NULL) CGPDFDocumentRelease(_document);
    [super finalize];
}

- (CGPDFDocumentRef) documentRef
{
    return _document;
}

- (CGRect) mediaBox
{
    return _mediaBox;
}

- (NSRect) bounds
{
    NSRect rotatedRect;

    // rotate the media box and calculate bounding box
    float sina   = sin (_rotation);
    float cosa   = cos (_rotation);
    float width  = NSWidth (_cropBox);
    float height = NSHeight(_cropBox);

    // calculate rotated x and y axis
    NSPoint rx = NSMakePoint( width  * cosa, width  * sina);
    NSPoint ry = NSMakePoint(-height * sina, height * cosa);

    // find delta width and height of rotated points
    rotatedRect.origin      = _cropBox.origin;
    rotatedRect.size.width  = ceil(fabs(rx.x - ry.x));
    rotatedRect.size.height = ceil(fabs(ry.y - rx.y));

    return rotatedRect;
}

- (void) adjustCTM:(CGContextRef)context
{
    // rotate the crop box and calculate bounding box
    float sina   = sin (-_rotation);
    float cosa   = cos (-_rotation);
    float width  = NSWidth (_cropBox);
    float height = NSHeight(_cropBox);

    // calculate rotated x and y edges of the corp box. if they're negative, it means part of the image has
    // been rotated outside of the bounds and we need to shift over the image so it lies inside the bounds again
    NSPoint rx = NSMakePoint( width  * cosa, width  * sina);
    NSPoint ry = NSMakePoint(-height * sina, height * cosa);

    // adjust so we are at the crop box origin
    CGContextTranslateCTM(context, floor(-MIN(0,MIN(rx.x, ry.x))), floor(-MIN(0,MIN(rx.y, ry.y))));

    // rotate -ve to remove rotation
    CGContextRotateCTM(context, -_rotation);

    // shift so we are completely within media box
    CGContextTranslateCTM(context, _mediaBox.origin.x - _cropBox.origin.x, _mediaBox.origin.y - _cropBox.origin.y);
}

- (void) setCurrentPage:(int)page
{
    if (page != _currentPage && page >= 0 && page < [self pageCount]) {

        CGRect r;

        _currentPage = page;

        // get media box (guaranteed)
        _mediaBox = CGPDFDocumentGetMediaBox(_document, page + 1);

        // get crop box (not always there). if not, use _mediaBox
        r = CGPDFDocumentGetCropBox(_document, page + 1);
        if (!CGRectIsEmpty(r)) {
            _cropBox = NSMakeRect(r.origin.x, r.origin.y, r.size.width, r.size.height);
        } else {
            _cropBox = NSMakeRect(_mediaBox.origin.x, _mediaBox.origin.y, _mediaBox.size.width, _mediaBox.size.height);
        }

        // get page rotation angle
        _rotation = CGPDFDocumentGetRotationAngle(_document, page + 1) * M_PI / 180.0;	// to radians
    }
}

- (int) currentPage
{
    return _currentPage;
}

- (int) pageCount
{
    return CGPDFDocumentGetNumberOfPages(_document);
}

@end

CGColorSpaceRef WebCGColorSpaceCreateRGB(void)
{
#ifdef COLORMATCH_EVERYTHING
#if BUILDING_ON_PANTHER
    return CGColorSpaceCreateWithName(kCGColorSpaceUserRGB);
#else // !BUILDING_ON_PANTHER
    return CGColorSpaceCreateDeviceRGB();
#endif // BUILDING_ON_PANTHER
#else // !COLORMATCH_EVERYTHING
#if BUILDING_ONPANTHER
    return CGColorSpaceCreateDeviceRGB();
#else // !BUILDING_ON_PANTHER
    return CGColorSpaceCreateDeviceRGB();
#endif // BUILDING_ON_PANTHER
#endif    
}

CGColorSpaceRef WebCGColorSpaceCreateGray(void)
{
#ifdef COLORMATCH_EVERYTHING
#if BUILDING_ON_PANTHER
    return CGColorSpaceCreateWithName(kCGColorSpaceUserGray);
#else // !BUILDING_ON_PANTHER
    return CGColorSpaceCreateDeviceGray();
#endif // BUILDING_ON_PANTHER
#else // !COLORMATCH_EVERYTHING
#if BUILDING_ONPANTHER
    return CGColorSpaceCreateDeviceGray();
#else // !BUILDING_ON_PANTHER
    return CGColorSpaceCreateDeviceGray();
#endif // BUILDING_ON_PANTHER
#endif    
}

CGColorSpaceRef WebCGColorSpaceCreateCMYK(void)
{
#ifdef COLORMATCH_EVERYTHING
#if BUILDING_ON_PANTHER
    return CGColorSpaceCreateWithName(kCGColorSpaceUserCMYK);
#else // !BUILDING_ON_PANTHER
    return CGColorSpaceCreateDeviceCMYK();
#endif // BUILDING_ON_PANTHER
#else // !COLORMATCH_EVERYTHING
#if BUILDING_ONPANTHER
    return CGColorSpaceCreateDeviceCMYK();
#else // !BUILDING_ON_PANTHER
    // FIXME: no device CMYK
    return CGColorSpaceCreateDeviceCMYK();
#endif // BUILDING_ON_PANTHER
#endif    
}
