/*	WebImageRenderer.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Cocoa/Cocoa.h>
#import <WebCore/WebCoreImageRenderer.h>

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
}

+ (void)stopAnimationsInView:(NSView *)aView;

@end
