/*	IFImageRenderer.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Cocoa/Cocoa.h>
#import <WebCoreImageRenderer.h>

@interface IFImageRenderer : NSImage <WebCoreImageRenderer>
{
    NSTimer *frameTimer;
    NSView *frameView;
    NSRect imageRect;
    NSRect targetRect;
    NSColor *patternColor;
}

+ (void)stopAnimationsInView: (NSView *)aView;

@end
