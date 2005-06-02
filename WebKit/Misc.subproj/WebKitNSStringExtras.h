/*
    WebKitNSStringExtras.h
    Private (SPI) header
    Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Cocoa/Cocoa.h>


@interface NSString (WebKitExtras)

- (void)_web_drawAtPoint:(NSPoint)point font:(NSFont *)font textColor:(NSColor *)textColor;
- (void)_web_drawDoubledAtPoint:(NSPoint)textPoint withTopColor:(NSColor *)topColor bottomColor:(NSColor *)bottomColor font:(NSFont *)font;

- (float)_web_widthWithFont:(NSFont *)font;

// Handles home directories that have symlinks in their paths.
// This works around 2774250.
- (NSString *)_web_stringByAbbreviatingWithTildeInPath;

- (NSString *)_web_stringByStrippingReturnCharacters;

+ (NSStringEncoding)_web_encodingForResource:(Handle)resource;

- (BOOL)_webkit_isCaseInsensitiveEqualToString:(NSString *)string;

@end
