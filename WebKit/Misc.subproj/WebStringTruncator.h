//
//  WebStringTruncator.h
//
//  Created by Darin Adler on Fri May 10 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//
//  Complete rewrite with API similar to slow truncator by Al Dul

#import <Foundation/Foundation.h>

@class NSFont;

@interface WebStringTruncator : NSObject
{
}

+ (NSString *)centerTruncateString:(NSString *)string toWidth:(float)maxWidth withFont:(NSFont *)font;

// Default font is [NSFont menuFontOfSize:0].
+ (NSString *)centerTruncateString:(NSString *)string toWidth:(float)maxWidth;

+ (NSString *)rightTruncateString:(NSString *)string toWidth:(float)maxWidth withFont:(NSFont *)font;

+ (float)widthOfString:(NSString *)string font:(NSFont *)font;

@end
