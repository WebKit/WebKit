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
    int lastStatus;
    unsigned lastLength;
    BOOL lastReturn;
}
- (BOOL)incrementalLoadWithBytes: (const void *)bytes length:(unsigned)length complete:(BOOL)isComplete;
- (void)beginAnimationInView: (NSView *)view inRect: (NSRect)ir fromRect: (NSRect)fr;
- (void)stopAnimation;
@end
