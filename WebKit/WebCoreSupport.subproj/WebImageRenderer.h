/*	WebImageRenderer.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Cocoa/Cocoa.h>

// Needed for CGCompositeOperation
#import <CoreGraphics/CGContextPrivate.h>

@protocol WebCoreImageRenderer;

//#if MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_3
//#define USE_CGIMAGEREF YES
//#endif

#ifdef USE_CGIMAGEREF
@class WebImageData;

@interface WebImageRenderer : NSObject <WebCoreImageRenderer>
{
    NSString *MIMEType;

    WebImageData *imageData;

    NSRect targetAnimationRect;
    
    NSSize adjustedSize;
    BOOL isSizeAdjusted;
}

- (id)initWithMIMEType:(NSString *)MIME;
- (id)initWithData:(NSData *)data MIMEType:(NSString *)MIME;
- (id)initWithContentsOfFile:(NSString *)filename;
- (NSString *)MIMEType;
+ (void)stopAnimationsInView:(NSView *)aView;
- (int)frameCount;
- (NSRect)targetAnimationRect;
- (void)resize:(NSSize)s;
- (NSSize)size;
- (NSData *)TIFFRepresentation;
- (NSImage *)image;

@end

#else


@interface WebImageRenderer : NSImage <WebCoreImageRenderer>
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
+ (void)stopAnimationsInView:(NSView *)aView;
- (int)frameCount;

- (NSString *)MIMEType;

@end

#endif
