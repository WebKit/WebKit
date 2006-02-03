/*
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "config.h"
#import <qstring.h>
#import "FloatRect.h"
#import "IntRect.h"
#import "Image.h"
#import "ImageAnimationObserver.h"
#import "ImageDecoder.h"
#import "Timer.h"
#import <kxmlcore/Vector.h>

#import "WebCoreImageRendererFactory.h"

namespace WebCore {

// ================================================
// PDF Images
// ================================================

class PDFDocumentImage {
public:
    PDFDocumentImage(NSData* data);
    ~PDFDocumentImage();
    
    CGPDFDocumentRef documentRef();
    CGRect mediaBox();
    NSRect bounds(); // adjust for rotation
    void setCurrentPage(int page);
    int currentPage();
    int pageCount();
    void adjustCTM(CGContextRef context);

    void draw(NSRect fromRect, NSRect toRect, Image::CompositeOperator op, float alpha, bool flipped, CGContextRef context);

private:
    CGPDFDocumentRef m_document;
    CGRect           m_mediaBox;
    NSRect           m_cropBox;
    float            m_rotation;
    int              m_currentPage;
};

static void releasePDFDocumentData(void *info, const void *data, size_t size)
{
    [(id)info autorelease];
}

PDFDocumentImage::PDFDocumentImage(NSData* data)
{
    if (data != nil) {
        CGDataProviderRef dataProvider = CGDataProviderCreateWithData([data retain], [data bytes], [data length], releasePDFDocumentData);
        m_document = CGPDFDocumentCreateWithProvider(dataProvider);
        CGDataProviderRelease(dataProvider);
    }
    m_currentPage = -1;
    setCurrentPage(0);
}

PDFDocumentImage::~PDFDocumentImage()
{
    CGPDFDocumentRelease(m_document);
}

CGPDFDocumentRef PDFDocumentImage::documentRef()
{
    return m_document;
}

CGRect PDFDocumentImage::mediaBox()
{
    return m_mediaBox;
}

NSRect PDFDocumentImage::bounds()
{
    NSRect rotatedRect;

    // rotate the media box and calculate bounding box
    float sina   = sinf(m_rotation);
    float cosa   = cosf(m_rotation);
    float width  = NSWidth(m_cropBox);
    float height = NSHeight(m_cropBox);

    // calculate rotated x and y axis
    NSPoint rx = NSMakePoint( width  * cosa, width  * sina);
    NSPoint ry = NSMakePoint(-height * sina, height * cosa);

    // find delta width and height of rotated points
    rotatedRect.origin      = m_cropBox.origin;
    rotatedRect.size.width  = ceilf(fabs(rx.x - ry.x));
    rotatedRect.size.height = ceilf(fabs(ry.y - rx.y));

    return rotatedRect;
}

void PDFDocumentImage::adjustCTM(CGContextRef context)
{
    // rotate the crop box and calculate bounding box
    float sina   = sinf(-m_rotation);
    float cosa   = cosf(-m_rotation);
    float width  = NSWidth (m_cropBox);
    float height = NSHeight(m_cropBox);

    // calculate rotated x and y edges of the corp box. if they're negative, it means part of the image has
    // been rotated outside of the bounds and we need to shift over the image so it lies inside the bounds again
    NSPoint rx = NSMakePoint( width  * cosa, width  * sina);
    NSPoint ry = NSMakePoint(-height * sina, height * cosa);

    // adjust so we are at the crop box origin
    CGContextTranslateCTM(context, floorf(-MIN(0,MIN(rx.x, ry.x))), floorf(-MIN(0,MIN(rx.y, ry.y))));

    // rotate -ve to remove rotation
    CGContextRotateCTM(context, -m_rotation);

    // shift so we are completely within media box
    CGContextTranslateCTM(context, m_mediaBox.origin.x - m_cropBox.origin.x, m_mediaBox.origin.y - m_cropBox.origin.y);
}

void PDFDocumentImage::setCurrentPage(int page)
{
    if (page != m_currentPage && page >= 0 && page < pageCount()) {

        CGRect r;

        m_currentPage = page;

        // get media box (guaranteed)
        m_mediaBox = CGPDFDocumentGetMediaBox(m_document, page + 1);

        // get crop box (not always there). if not, use _mediaBox
        r = CGPDFDocumentGetCropBox(m_document, page + 1);
        if (!CGRectIsEmpty(r)) {
            m_cropBox = NSMakeRect(r.origin.x, r.origin.y, r.size.width, r.size.height);
        } else {
            m_cropBox = NSMakeRect(m_mediaBox.origin.x, m_mediaBox.origin.y, m_mediaBox.size.width, m_mediaBox.size.height);
        }

        // get page rotation angle
        m_rotation = CGPDFDocumentGetRotationAngle(m_document, page + 1) * M_PI / 180.0; // to radians
    }
}

int PDFDocumentImage::currentPage()
{
    return m_currentPage;
}

int PDFDocumentImage::pageCount()
{
    return CGPDFDocumentGetNumberOfPages(m_document);
}

void PDFDocumentImage::draw(NSRect srcRect, NSRect dstRect, Image::CompositeOperator op, 
                            float alpha, bool flipped, CGContextRef context)
{
    CGContextSaveGState(context);

    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    [[NSGraphicsContext graphicsContextWithGraphicsPort:context flipped:NO] setCompositingOperation:(NSCompositingOperation)op];
    [pool release];

    // Scale and translate so the document is rendered in the correct location.
    float hScale = dstRect.size.width  / srcRect.size.width;
    float vScale = dstRect.size.height / srcRect.size.height;

    CGContextTranslateCTM(context, dstRect.origin.x - srcRect.origin.x * hScale, dstRect.origin.y - srcRect.origin.y * vScale);
    CGContextScaleCTM(context, hScale, vScale);

    // Reverse if flipped image.
    if (flipped) {
        CGContextScaleCTM(context, 1, -1);
        CGContextTranslateCTM(context, 0, -dstRect.size.height);
    }

    // Clip to destination in case we are imaging part of the source only
    CGContextClipToRect(context, CGRectIntegral(*(CGRect*)&srcRect));

    // and draw
    if (m_document) {
        CGContextSaveGState(context);
        // Rotate translate image into position according to doc properties.
        adjustCTM(context);

        // Media box may have non-zero origin which we ignore. CGPDFDocumentShowPage pages start
        // at 1, not 0.
        CGContextDrawPDFDocument(context, CGRectMake(0, 0, m_mediaBox.size.width, m_mediaBox.size.height), m_document, 1);

        CGContextRestoreGState(context);
    }

    // done with our fancy transforms
    CGContextRestoreGState(context);
}

// ================================================
// FrameData Class
// ================================================

struct FrameData {
    FrameData()
      :m_frame(0), m_duration(0) 
    {}
    
    ~FrameData()
    { 
        clear();
    }

    void clear()
    { 
        if (m_frame)
            CFRelease(m_frame);
        m_frame = 0;
        m_duration = 0.;
    }

    CGImageRef m_frame;
    float m_duration;
};

// ================================================
// ImageData Class
// ================================================

class ImageData {
public:
    ImageData(Image*);
    ~ImageData();

    bool isNull() const;
    IntSize size() const;
    void setIsPDF() { m_isPDF = true; }

    // |bytes| contains all of the data that we have seen so far.
    bool setData(const ByteArray& bytes, bool allDataReceived);
    bool setCFData(CFDataRef data, bool allDataReceived);

    size_t currentFrame() const { return m_currentFrame; }
    size_t frameCount() const;
    CGImageRef frameAtIndex(size_t index);
    NSImage* getNSImage();
    CFDataRef getTIFFRepresentation();

    float frameDurationAtIndex(size_t index);
    
    bool shouldAnimate() const;
    void startAnimation();
    void stopAnimation();
    void resetAnimation();
    void advanceAnimation(Timer<ImageData>* timer);

    void drawInRect(const FloatRect& dstRect, const FloatRect& srcRect,
                    Image::CompositeOperator compositeOp, CGContextRef context);
    void tileInRect(const FloatRect& dstRect, const FloatPoint& point, CGContextRef context);
    void scaleAndTileInRect(const FloatRect& dstRect, const FloatRect& srcRect,
                            Image::TileRule hRule, Image::TileRule vRule, CGContextRef context);

private:
    bool isSizeAvailable();
    
    void invalidateData();
    void cacheFrame(size_t index);

    void checkForSolidColor(CGImageRef image);
    void fillSolidColorInRect(CGRect rect, Image::CompositeOperator compositeOp, CGContextRef context);

    void setCompositingOperation(CGContextRef context, Image::CompositeOperator compositeOp);
    
    ByteArray m_data; // The encoded raw data for the image.
    Image* m_image;
    ImageDecoder m_decoder;
    mutable IntSize m_size; // The size to use for the overall image (will just be the size of the first image).
    
    size_t m_currentFrame; // The index of the current frame of animation.
    Vector<FrameData> m_frames; // An array of the cached frames of the animation. We have to ref frames to pin them in the cache.
    NSImage* m_nsImage; // A cached NSImage of frame 0. Only built lazily if someone actually queries for one.
    CFDataRef m_tiffRep; // Cached TIFF rep for frame 0.  Only built lazily if someone queries for one.

    Timer<ImageData>* m_frameTimer;
    int m_repetitionCount; // How many total animation loops we should do.
    int m_repetitionsComplete;  // How many repetitions we've finished.

    CGColorRef m_solidColor; // Will be 0 if transparent.
    bool m_isSolidColor : 1; // If the image is 1x1 with no alpha, we can just do a rect fill when painting/tiling.

    bool m_animatingImageType : 1;  // Whether or not we're an image type that is capable of animating (GIF).
    bool m_animationFinished : 1;  // Whether or not we've completed the entire animation.

    mutable bool m_haveSize : 1; // Whether or not our |m_size| member variable has the final overall image size yet.
    bool m_sizeAvailable : 1; // Whether or not we can obtain the size of the first image frame yet from ImageIO.
    bool m_isPDF : 1;
    
    PDFDocumentImage* m_PDFDoc;
};

ImageData::ImageData(Image* image) 
     :m_image(image), m_currentFrame(0), m_frames(0), m_nsImage(0), m_tiffRep(0),
      m_frameTimer(0), m_repetitionCount(0), m_repetitionsComplete(0),
      m_solidColor(0), m_isSolidColor(0), m_animatingImageType(true), m_animationFinished(0),
      m_haveSize(false), m_sizeAvailable(false), 
      m_isPDF(false), m_PDFDoc(0)
{}

ImageData::~ImageData()
{
    invalidateData();
    stopAnimation();
    delete m_PDFDoc;
}

void ImageData::invalidateData()
{
    // Destroy the cached images and release them.
    if (m_frames) {
        size_t count = m_frames.size();
        if (count) {
            m_frames.last().clear();
            if (count == 1) {
                if (m_nsImage) {
                    [m_nsImage release];
                    m_nsImage = 0;
                }
            
                if (m_tiffRep) {
                    CFRelease(m_tiffRep);
                    m_tiffRep = 0;
                }
            
                m_isSolidColor = false;
                if (m_solidColor) {
                    CFRelease(m_solidColor);
                    m_solidColor = 0;
                }
            }
        }
    }
}

void ImageData::cacheFrame(size_t index)
{
    size_t numFrames = frameCount();
    if (!m_frames.size() && shouldAnimate()) {            
        // Snag the repetition count.
        m_repetitionCount = m_decoder.repetitionCount();
        if (m_repetitionCount == cAnimationNone)
            m_animatingImageType = false;
    }
    
    if (m_frames.size() < numFrames)
        m_frames.resize(numFrames);

    m_frames[index].m_frame = m_decoder.createFrameAtIndex(index);
    if (shouldAnimate())
        m_frames[index].m_duration = m_decoder.frameDurationAtIndex(index);
}

void ImageData::checkForSolidColor(CGImageRef image)
{
    m_isSolidColor = false;
    if (m_solidColor) {
        CFRelease(m_solidColor);
        m_solidColor = 0;
    }
    
    // Currently we only check for solid color in the important special case of a 1x1 image.
    if (image && CGImageGetWidth(image) == 1 && CGImageGetHeight(image) == 1) {
        float pixel[4]; // RGBA
        CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
// This #if won't be needed once the CG header that includes kCGBitmapByteOrder32Host is included in the OS.
#if __ppc__
        CGContextRef bmap = CGBitmapContextCreate(&pixel, 1, 1, 8*sizeof(float), sizeof(pixel), space,
            kCGImageAlphaPremultipliedLast | kCGBitmapFloatComponents);
#else
        CGContextRef bmap = CGBitmapContextCreate(&pixel, 1, 1, 8*sizeof(float), sizeof(pixel), space,
            kCGImageAlphaPremultipliedLast | kCGBitmapFloatComponents | kCGBitmapByteOrder32Host);
#endif
        if (bmap) {
            setCompositingOperation(bmap, Image::CompositeCopy);
            
            CGRect dst = { {0, 0}, {1, 1} };
            CGContextDrawImage(bmap, dst, image);
            if (pixel[3] > 0)
                m_solidColor = CGColorCreate(space,pixel);
            m_isSolidColor = true;
            CFRelease(bmap);
        } 
        CFRelease(space);
    }
}

bool ImageData::isNull() const
{
    return size().isEmpty();
}

IntSize ImageData::size() const
{
    if (m_isPDF) {
        if (m_PDFDoc) {
            CGRect mediaBox = m_PDFDoc->mediaBox();
            return IntSize((int)mediaBox.size.width, (int)mediaBox.size.height);
        }
    } else if (m_sizeAvailable && !m_haveSize && frameCount() > 0) {
        m_size = m_decoder.size();
        m_haveSize = true;
    }
    return m_size;
}

bool ImageData::setData(const ByteArray& bytes, bool allDataReceived)
{
    int length = bytes.count();
    if (length == 0)
        return true;

#ifdef kImageBytesCutoff
    // This is a hack to help with testing display of partially-loaded images.
    // To enable it, define kImageBytesCutoff to be a size smaller than that of the image files
    // being loaded. They'll never finish loading.
    if (length > kImageBytesCutoff) {
        length = kImageBytesCutoff;
        allDataReceived = false;
    }
#endif
    
    // Avoid the extra copy of bytes by just handing the byte array directly to a CFDataRef.
    // We will keep these bytes alive in our m_data variable.
    CFDataRef data = CFDataCreateWithBytesNoCopy(0, (const UInt8*)bytes.data(), length, kCFAllocatorNull);
    bool result = setCFData(data, allDataReceived);
    CFRelease(data);
    
    m_data = bytes;
    
    return result;
}

bool ImageData::setCFData(CFDataRef data, bool allDataReceived)
{
    if (m_isPDF) {
        if (allDataReceived && !m_PDFDoc)
            m_PDFDoc = new PDFDocumentImage((NSData*)data);
        return true;
    }
        
    invalidateData();
    
    // Feed all the data we've seen so far to the image decoder.
    m_decoder.setData(data, allDataReceived);
    
    // Image properties will not be available until the first frame of the file
    // reaches kCGImageStatusIncomplete.
    return isSizeAvailable();
}

size_t ImageData::frameCount() const
{
    return m_decoder.frameCount();
}

bool ImageData::isSizeAvailable()
{
    if (m_sizeAvailable)
        return true;

    m_sizeAvailable = m_decoder.isSizeAvailable();

    return m_sizeAvailable;

}

CGImageRef ImageData::frameAtIndex(size_t index)
{
    if (index >= frameCount())
        return 0;

    if (index >= m_frames.size() || !m_frames[index].m_frame)
        cacheFrame(index);

    return m_frames[index].m_frame;
}

CFDataRef ImageData::getTIFFRepresentation()
{
    if (m_tiffRep)
        return m_tiffRep;

    CGImageRef cgImage = frameAtIndex(0);
    if (!cgImage)
        return 0;
   
    CFMutableDataRef data = CFDataCreateMutable(0, 0);
    // FIXME:  Use type kCGImageTypeIdentifierTIFF constant once is becomes available in the API
    CGImageDestinationRef destination = CGImageDestinationCreateWithData(data, CFSTR("public.tiff"), 1, 0);
    if (destination) {
        CGImageDestinationAddImage(destination, cgImage, 0);
        CGImageDestinationFinalize(destination);
        CFRelease(destination);
    }

    m_tiffRep = data;
    return m_tiffRep;
}

NSImage* ImageData::getNSImage()
{
    if (m_nsImage)
        return m_nsImage;

    CFDataRef data = getTIFFRepresentation();
    if (!data)
        return 0;
    
    m_nsImage = [[NSImage alloc] initWithData:(NSData*)data];
    return m_nsImage;
}

float ImageData::frameDurationAtIndex(size_t index)
{
    if (index >= frameCount())
        return 0;

    if (index >= m_frames.size() || !m_frames[index].m_frame)
        cacheFrame(index);

    return m_frames[index].m_duration;
}

bool ImageData::shouldAnimate() const
{
    return (m_animatingImageType && frameCount() > 1 && !m_animationFinished && m_image->animationObserver());
}

void ImageData::startAnimation()
{
    if (m_frameTimer || !shouldAnimate())
        return;

    m_frameTimer = new Timer<ImageData>(this, &ImageData::advanceAnimation);
    m_frameTimer->startOneShot(frameDurationAtIndex(m_currentFrame));
}

void ImageData::stopAnimation()
{
    // This timer is used to animate all occurrences of this image.  Don't invalidate
    // the timer unless all renderers have stopped drawing.
    delete m_frameTimer;
    m_frameTimer = 0;
}

void ImageData::resetAnimation()
{
    stopAnimation();
    m_currentFrame = 0;
    m_repetitionsComplete = 0;
    m_animationFinished = false;
}

void ImageData::advanceAnimation(Timer<ImageData>* timer)
{
    // Stop the animation.
    stopAnimation();
    
    // See if anyone is still paying attention to this animation.  If not, we don't
    // advance and will simply pause the animation.
    if (m_image->animationObserver()->shouldStopAnimation(m_image))
        return;

    m_currentFrame++;
    if (m_currentFrame >= frameCount()) {
        m_repetitionsComplete += 1;
        if (m_repetitionCount && m_repetitionsComplete >= m_repetitionCount) {
            m_animationFinished = YES;
            m_currentFrame--;
            return;
        }
        m_currentFrame = 0;
    }

    // Notify our observer that the animation has advanced.
    m_image->animationObserver()->animationAdvanced(m_image);
        
    // Kick off a timer to move to the next frame.
    m_frameTimer = new Timer<ImageData>(this, &ImageData::advanceAnimation);
    m_frameTimer->startOneShot(frameDurationAtIndex(m_currentFrame));
}

void ImageData::setCompositingOperation(CGContextRef context, Image::CompositeOperator op)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    [[NSGraphicsContext graphicsContextWithGraphicsPort:context flipped:NO] setCompositingOperation:(NSCompositingOperation)op];
    [pool release];
}

void ImageData::fillSolidColorInRect(CGRect rect, Image::CompositeOperator op, CGContextRef context)
{
    if (m_solidColor) {
        CGContextSaveGState(context);
        CGContextSetFillColorWithColor(context, m_solidColor);
        setCompositingOperation(context, op);
        CGContextFillRect(context, rect);
        CGContextRestoreGState(context);
    }
}

void ImageData::drawInRect(const FloatRect& dstRect, const FloatRect& srcRect,
                           Image::CompositeOperator compositeOp, CGContextRef context)
{
    if (m_isPDF) {
        if (m_PDFDoc)
            m_PDFDoc->draw(srcRect, dstRect, compositeOp, 1.0, true, context);
        return;
    } 
    
    if (!m_decoder.initialized())
        return;
    
    CGRect fr = srcRect;
    CGRect ir = dstRect;

    CGImageRef image = frameAtIndex(m_currentFrame);
    if (!image) // If it's too early we won't have an image yet.
        return;

    if (m_isSolidColor && m_currentFrame == 0)
        return fillSolidColorInRect(ir, compositeOp, context);

    CGContextSaveGState(context);
        
    // Get the height (in adjusted, i.e. scaled, coords) of the portion of the image
    // that is currently decoded.  This could be less that the actual height.
    CGSize selfSize = size();                          // full image size, in pixels
    float curHeight = CGImageGetHeight(image);         // height of loaded portion, in pixels
    
    CGSize adjustedSize = selfSize;
    if (curHeight < selfSize.height) {
        adjustedSize.height *= curHeight / selfSize.height;

        // Is the amount of available bands less than what we need to draw?  If so,
        // we may have to clip 'fr' if it goes outside the available bounds.
        if (CGRectGetMaxY(fr) > adjustedSize.height) {
            float frHeight = adjustedSize.height - fr.origin.y; // clip fr to available bounds
            if (frHeight <= 0)
                return;                                             // clipped out entirely
            ir.size.height *= (frHeight / fr.size.height);    // scale ir proportionally to fr
            fr.size.height = frHeight;
        }
    }

    // Flip the coords.
    setCompositingOperation(context, compositeOp);
    CGContextTranslateCTM(context, ir.origin.x, ir.origin.y);
    CGContextScaleCTM(context, 1, -1);
    CGContextTranslateCTM(context, 0, -ir.size.height);
    
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
        
        image = CGImageCreateWithImageInRect(image, fr);
        if (image) {
            CGContextDrawImage(context, ir, image);
            CFRelease(image);
        }
    } else // Draw the whole image.
        CGContextDrawImage(context, ir, image);

    CGContextRestoreGState(context);
    
    startAnimation();
}

static void drawPattern(void * info, CGContextRef context)
{
    ImageData *data = (ImageData *)info;
    CGImageRef image = data->frameAtIndex(data->currentFrame());
    float w = CGImageGetWidth(image);
    float h = CGImageGetHeight(image);
    CGContextDrawImage(context, CGRectMake(0, data->size().height() - h, w, h), image);    
}

static const CGPatternCallbacks patternCallbacks = { 0, drawPattern, NULL };

void ImageData::tileInRect(const FloatRect& dstRect, const FloatPoint& srcPoint, CGContextRef context)
{
    CGImageRef image = frameAtIndex(m_currentFrame);
    if (!image)
        return;

    if (m_currentFrame == 0 && m_isSolidColor)
        return fillSolidColorInRect(dstRect, Image::CompositeSourceOver, context);

    CGSize tileSize = size();
    CGRect rect = dstRect;
    CGPoint point = srcPoint;

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

        drawInRect(dstRect, FloatRect(fromRect), Image::CompositeSourceOver, context);
        return;
    }

    CGPatternRef pattern = CGPatternCreate(this, CGRectMake(0, 0, tileSize.width, tileSize.height),
                                           CGAffineTransformIdentity, tileSize.width, tileSize.height, 
                                           kCGPatternTilingConstantSpacing, true, &patternCallbacks);
    
    if (pattern) {
        CGContextSaveGState(context);

        CGPoint tileOrigin = CGPointMake(oneTileRect.origin.x, oneTileRect.origin.y);
        [[WebCoreImageRendererFactory sharedFactory] setPatternPhaseForContext:context inUserSpace:tileOrigin];

        CGColorSpaceRef patternSpace = CGColorSpaceCreatePattern(NULL);
        CGContextSetFillColorSpace(context, patternSpace);
        CGColorSpaceRelease(patternSpace);

        float patternAlpha = 1;
        CGContextSetFillPattern(context, pattern, &patternAlpha);

        setCompositingOperation(context, Image::CompositeSourceOver);

        CGContextFillRect(context, rect);

        CGContextRestoreGState(context);

        CGPatternRelease(pattern);
    }
    
    startAnimation();
}

void ImageData::scaleAndTileInRect(const FloatRect& dstRect, const FloatRect& srcRect,
                                   Image::TileRule hRule, Image::TileRule vRule, CGContextRef context)
{

    CGImageRef image = frameAtIndex(m_currentFrame);
    if (!image)
        return;

    if (m_currentFrame == 0 && m_isSolidColor)
        return fillSolidColorInRect(dstRect, Image::CompositeSourceOver, context);

    CGContextSaveGState(context);
    CGSize tileSize = srcRect.size();
    CGRect ir = dstRect;
    CGRect fr = srcRect;

    // Now scale the slice in the appropriate direction using an affine transform that we will pass into
    // the pattern.
    float scaleX = 1.0f, scaleY = 1.0f;

    if (hRule == Image::StretchTile)
        scaleX = ir.size.width / fr.size.width;
    if (vRule == Image::StretchTile)
        scaleY = ir.size.height / fr.size.height;
    
    if (hRule == Image::RepeatTile)
        scaleX = scaleY;
    if (vRule == Image::RepeatTile)
        scaleY = scaleX;
        
    if (hRule == Image::RoundTile) {
        // Complicated math ensues.
        float imageWidth = fr.size.width * scaleY;
        float newWidth = ir.size.width / ceilf(ir.size.width / imageWidth);
        scaleX = newWidth / fr.size.width;
    }
    
    if (vRule == Image::RoundTile) {
        // More complicated math ensues.
        float imageHeight = fr.size.height * scaleX;
        float newHeight = ir.size.height / ceilf(ir.size.height / imageHeight);
        scaleY = newHeight / fr.size.height;
    }
    
    CGAffineTransform patternTransform = CGAffineTransformMakeScale(scaleX, scaleY);

    // Possible optimization:  We may want to cache the CGPatternRef    
    CGPatternRef pattern = CGPatternCreate(this, CGRectMake(fr.origin.x, fr.origin.y, tileSize.width, tileSize.height),
                                           patternTransform, tileSize.width, tileSize.height, 
                                           kCGPatternTilingConstantSpacing, true, &patternCallbacks);
    if (pattern) {
        // We want to construct the phase such that the pattern is centered (when stretch is not
        // set for a particular rule).
        float hPhase = scaleX * fr.origin.x;
        float vPhase = scaleY * (tileSize.height - fr.origin.y);
        if (hRule == Image::RepeatTile)
            hPhase -= fmodf(ir.size.width, scaleX * tileSize.width) / 2.0f;
        if (vRule == Image::RepeatTile)
            vPhase -= fmodf(ir.size.height, scaleY * tileSize.height) / 2.0f;
        
        CGPoint tileOrigin = CGPointMake(ir.origin.x - hPhase, ir.origin.y - vPhase);
        [[WebCoreImageRendererFactory sharedFactory] setPatternPhaseForContext:context inUserSpace:tileOrigin];

        CGColorSpaceRef patternSpace = CGColorSpaceCreatePattern(NULL);
        CGContextSetFillColorSpace(context, patternSpace);
        CGColorSpaceRelease(patternSpace);

        float patternAlpha = 1;
        CGContextSetFillPattern(context, pattern, &patternAlpha);

        setCompositingOperation(context, Image::CompositeSourceOver);
        
        CGContextFillRect(context, ir);

        CGPatternRelease (pattern);
    }

    CGContextRestoreGState(context);

    startAnimation();
}


// ================================================
// Image Class
// ================================================

Image* Image::loadResource(const char *name)
{
    NSData* namedImageData = [[WebCoreImageRendererFactory sharedFactory] imageDataForName:[NSString stringWithCString:name]];
    if (namedImageData) {
        Image* image = new Image;
        image->m_data = new ImageData(image);
        image->m_data->setCFData((CFDataRef)namedImageData, true);
        return image;
    }
    return 0;
}

bool Image::supportsType(const QString& type)
{
    return [[[WebCoreImageRendererFactory sharedFactory] supportedMIMETypes] containsObject:type.getNSString()];
}

Image::Image()
:m_data(0), m_animationObserver(0)
{
}

Image::Image(const QString& type, ImageAnimationObserver* observer)
{
    m_data = new ImageData(this);
    if (type == "application/pdf")
        m_data->setIsPDF();
    m_animationObserver = observer;
}

Image::~Image()
{
    delete m_data;
}

CGImageRef Image::getCGImageRef() const
{
    if (m_data)
        return m_data->frameAtIndex(0);
    return 0;
}

NSImage* Image::getNSImage() const
{
    if (m_data)
        return m_data->getNSImage();
    return 0;
}

CFDataRef Image::getTIFFRepresentation() const
{
    if (m_data)
        return m_data->getTIFFRepresentation();
    return 0;
}

void Image::resetAnimation() const
{
    if (m_data)
        m_data->resetAnimation();
}

bool Image::setData(const ByteArray& bytes, bool allDataReceived)
{
    // Make sure we have some data.
    if (!m_data)
        return true;
    
    return m_data->setData(bytes, allDataReceived);
}

bool Image::isNull() const
{
    return m_data ? m_data->isNull() : true;
}

IntSize Image::size() const
{
    return m_data ? m_data->size() : IntSize();
}

IntRect Image::rect() const
{
    return m_data ? IntRect(IntPoint(), m_data->size()) : IntRect();
}

int Image::width() const
{
    return size().width();
}

int Image::height() const
{
    return size().height();
}

const int NUM_COMPOSITE_OPERATORS = 14;

struct CompositeOperatorEntry
{
    const char *name;
    Image::CompositeOperator value;
};

struct CompositeOperatorEntry compositeOperators[NUM_COMPOSITE_OPERATORS] = {
    { "clear", Image::CompositeClear },
    { "copy", Image::CompositeCopy },
    { "source-over", Image::CompositeSourceOver },
    { "source-in", Image::CompositeSourceIn },
    { "source-out", Image::CompositeSourceOut },
    { "source-atop", Image::CompositeSourceAtop },
    { "destination-over", Image::CompositeDestinationOver },
    { "destination-in", Image::CompositeDestinationIn },
    { "destination-out", Image::CompositeDestinationOut },
    { "destination-atop", Image::CompositeDestinationAtop },
    { "xor", Image::CompositeXOR },
    { "darker", Image::CompositePlusDarker },
    { "highlight", Image::CompositeHighlight },
    { "lighter", Image::CompositePlusLighter }
};

Image::CompositeOperator Image::compositeOperatorFromString(const QString &aString)
{
    CompositeOperator op = CompositeSourceOver;
    
    if (aString.length()) {
        const char *operatorString = aString.ascii();
        for (int i = 0; i < NUM_COMPOSITE_OPERATORS; i++) {
            if (strcasecmp (operatorString, compositeOperators[i].name) == 0) {
                return compositeOperators[i].value;
            }
        }
    }
    return op;
}

// Drawing Routines
static CGContextRef graphicsContext(void* context)
{
    if (!context)
        return (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];
    return (CGContextRef)context;
}

void Image::drawInRect(const FloatRect& dstRect, const FloatRect& srcRect,
                       CompositeOperator compositeOp, void* context) const
{
    if (m_data)
        m_data->drawInRect(dstRect, srcRect, compositeOp, graphicsContext(context));
}

void Image::tileInRect(const FloatRect& dstRect, const FloatPoint& point, void* context) const
{
    if (m_data)
        m_data->tileInRect(dstRect, point, graphicsContext(context));
}

void Image::scaleAndTileInRect(const FloatRect& dstRect, const FloatRect& srcRect,
                               TileRule hRule, TileRule vRule, void* context) const
{
    if (m_data)
        m_data->scaleAndTileInRect(dstRect, srcRect, hRule, vRule, graphicsContext(context));
}

}
