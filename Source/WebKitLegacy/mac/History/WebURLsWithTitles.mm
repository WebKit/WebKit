/*
 * Copyright (C) 2005 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#if !PLATFORM(IOS_FAMILY)

#import "WebURLsWithTitles.h"

#import <WebKitLegacy/WebKitNSStringExtras.h>
#import <WebKitLegacy/WebNSURLExtras.h>
#import <wtf/cocoa/TypeCastsCocoa.h>

@implementation WebURLsWithTitles

+ (NSArray *)arrayWithIFURLsWithTitlesPboardType
{
    // Make a canned array so we don't construct it on the fly over and over.
    static NSArray *cannedArray = nil;

    if (cannedArray == nil)
        cannedArray = [@[WebURLsWithTitlesPboardType] retain];

    return cannedArray;
}

+ (void)writeURLs:(NSArray *)URLs andTitles:(NSArray *)titles toPasteboard:(NSPasteboard *)pasteboard
{
    RetainPtr<NSMutableArray> urlStrings;
    RetainPtr<NSMutableArray> titlesOrEmptyStrings;
    NSUInteger index, count;

    count = [URLs count];
    if (!count)
        return;

    if ([pasteboard availableTypeFromArray:[self arrayWithIFURLsWithTitlesPboardType]] == nil)
        return;

    if (count != [titles count])
        titles = nil;

    urlStrings = [NSMutableArray arrayWithCapacity:count];
    titlesOrEmptyStrings = [NSMutableArray arrayWithCapacity:count];
    for (index = 0; index < count; ++index) {
        [urlStrings addObject:[[URLs objectAtIndex:index] _web_originalDataAsString]];
        [titlesOrEmptyStrings addObject:(titles == nil) ? @"" : [[titles objectAtIndex:index] _webkit_stringByTrimmingWhitespace]];
    }

    [pasteboard setPropertyList:@[urlStrings.get(), titlesOrEmptyStrings.get()] forType:WebURLsWithTitlesPboardType];
}

+ (NSArray *)titlesFromPasteboard:(NSPasteboard *)pasteboard
{
    if ([pasteboard availableTypeFromArray:[self arrayWithIFURLsWithTitlesPboardType]] == nil)
        return nil;

    return [[pasteboard propertyListForType:WebURLsWithTitlesPboardType] objectAtIndex:1];
}

+ (NSArray *)URLsFromPasteboard:(NSPasteboard *)pasteboard
{
    RetainPtr<NSArray> urlStrings;
    RetainPtr<NSMutableArray> URLs;
    unsigned index, count;

    if ([pasteboard availableTypeFromArray:[self arrayWithIFURLsWithTitlesPboardType]] == nil)
        return nil;

    urlStrings = dynamic_objc_cast<NSArray>([[pasteboard propertyListForType:WebURLsWithTitlesPboardType] objectAtIndex:0]);
    count = [urlStrings count];
    URLs = [NSMutableArray arrayWithCapacity:count];
    for (index = 0; index < count; ++index)
        [URLs addObject:[NSURL _web_URLWithDataAsString:dynamic_objc_cast<NSString>([urlStrings objectAtIndex:index])]];

    return URLs.autorelease();
}

@end

#endif // !PLATFORM(IOS_FAMILY)
