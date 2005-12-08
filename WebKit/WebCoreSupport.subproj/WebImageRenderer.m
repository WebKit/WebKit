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

#import <WebCore/WebCoreImageRenderer.h>
#import <WebKit/WebAssertions.h>
#import <WebKit/WebGraphicsBridge.h>
#import <WebKit/WebHTMLView.h>
#import <WebKit/WebImageData.h>
#import <WebKit/WebImageRendererFactory.h>
#import <WebKit/WebImageView.h>
#import <WebKit/WebNSObjectExtras.h>
#import <WebKitSystemInterface.h>

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

    if (isSizeAdjusted) {
        [imageData drawImageAtIndex:[imageData currentFrame] inRect:CGRectMake(ir.origin.x, ir.origin.y, ir.size.width, ir.size.height) 
                fromRect:CGRectMake(fr.origin.x, fr.origin.y, fr.size.width, fr.size.height) 
                adjustedSize:CGSizeMake(adjustedSize.width, adjustedSize.height)
                compositeOperation:operator context:aContext];
    }
    else {
        [imageData drawImageAtIndex:[imageData currentFrame] inRect:CGRectMake(ir.origin.x, ir.origin.y, ir.size.width, ir.size.height) 
                fromRect:CGRectMake(fr.origin.x, fr.origin.y, fr.size.width, fr.size.height) 
                compositeOperation:operator context:aContext];
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

- (void)scaleAndTileInRect:(NSRect)ir fromRect:(NSRect)fr withHorizontalTileRule:(WebImageTileRule)hRule 
      withVerticalTileRule:(WebImageTileRule)vRule context:(CGContextRef)context
{
    if (context == 0)
        context = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];

    [imageData scaleAndTileInRect:CGRectMake(ir.origin.x, ir.origin.y, ir.size.width, ir.size.height)
            fromRect:CGRectMake(fr.origin.x, fr.origin.y, fr.size.width, fr.size.height) 
            withHorizontalTileRule:hRule withVerticalTileRule:vRule context:context];

    targetAnimationRect = ir;
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

- (void)setAnimationRect:(NSRect)r
{
    targetAnimationRect = r;
    [self _startOrContinueAnimationIfNecessary];
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

//------------------------------------------------------------------------------------

@implementation WebPDFDocument

static void ReleasePDFDocumentData(void *info, const void *data, size_t size)
{
    [(id)info autorelease];
}

- (id)initWithData:(NSData*)data
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
