/*	WebImageRenderer.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Cocoa/Cocoa.h>

@protocol WebCoreImageRenderer;

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

    BOOL sawGIFExtensionSignature;
    char GIFExtensionBuffer[10];
    int GIFExtensionBufferLength;
}

+ (void)stopAnimationsInView:(NSView *)aView;

@end
