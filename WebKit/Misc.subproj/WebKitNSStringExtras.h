/*
    WebKitNSStringExtras.h
    Private (SPI) header
    Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Cocoa/Cocoa.h>


@interface NSString (WebKitExtras)

- (void)_web_drawAtPoint:(NSPoint)point font: (NSFont *)font textColor:(NSColor *)textColor;

- (float)_web_widthWithFont:(NSFont *)font;

@end
