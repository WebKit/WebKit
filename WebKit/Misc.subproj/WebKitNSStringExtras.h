/*
    WebKitNSStringExtras.h
    Private (SPI) header
    Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Cocoa/Cocoa.h>


@interface NSString (WebKitExtras)

- (void)drawString:(NSString *)string atPoint:(NSPoint)point font: (NSFont *)font textColor:(NSColor *)textColor;

- (float)widthForString:(NSString *)string font: (NSFont *)font;

@end
