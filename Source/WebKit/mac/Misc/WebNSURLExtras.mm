/*
 * Copyright (C) 2005, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov (ap@nypop.com)
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

#import "WebNSURLExtras.h"

#import "WebKitNSStringExtras.h"
#import "WebLocalizableStrings.h"
#import "WebNSDataExtras.h"
#import "WebSystemInterface.h"
#import <Foundation/NSURLRequest.h>
#import <WebCore/URL.h>
#import <WebCore/LoaderNSURLExtras.h>
#import <WebCore/WebCoreNSURLExtras.h>
#import <WebKitSystemInterface.h>
#import <wtf/Assertions.h>
#import <wtf/ObjcRuntimeExtras.h>
#import <unicode/uchar.h>
#import <unicode/uscript.h>

#if PLATFORM(IOS)
#import <Foundation/NSString_NSURLExtras.h>
#endif

using namespace WebCore;
using namespace WTF;

#if PLATFORM(IOS)
// Fake URL scheme.
#define WebDataProtocolScheme @"webkit-fake-url"
#endif

#define URL_BYTES_BUFFER_LENGTH 2048

@implementation NSURL (WebNSURLExtras)

+ (NSURL *)_web_URLWithUserTypedString:(NSString *)string relativeToURL:(NSURL *)URL
{
    return URLWithUserTypedString(string, URL);
}

+ (NSURL *)_web_URLWithUserTypedString:(NSString *)string
{
    return URLWithUserTypedString(string, nil);
}

+ (NSURL *)_web_URLWithDataAsString:(NSString *)string
{
    if (string == nil) {
        return nil;
    }
    return [self _web_URLWithDataAsString:string relativeToURL:nil];
}

+ (NSURL *)_web_URLWithDataAsString:(NSString *)string relativeToURL:(NSURL *)baseURL
{
    if (string == nil) {
        return nil;
    }
    string = [string _webkit_stringByTrimmingWhitespace];
    NSData *data = [string dataUsingEncoding:NSISOLatin1StringEncoding];
    return URLWithData(data, baseURL);
}

+ (NSURL *)_web_URLWithData:(NSData *)data
{
    return URLWithData(data, nil);
}      

+ (NSURL *)_web_URLWithData:(NSData *)data relativeToURL:(NSURL *)baseURL
{
    return URLWithData(data, baseURL);
}

- (NSData *)_web_originalData
{
    return originalURLData(self);
}

- (NSString *)_web_originalDataAsString
{
    return [[[NSString alloc] initWithData:originalURLData(self) encoding:NSISOLatin1StringEncoding] autorelease];
}

- (NSString *)_web_userVisibleString
{
    return userVisibleString(self);
}

- (BOOL)_web_isEmpty
{
    if (!CFURLGetBaseURL((CFURLRef)self))
        return CFURLGetBytes((CFURLRef)self, NULL, 0) == 0;
    return [originalURLData(self) length] == 0;
}

- (const char *)_web_URLCString
{
    NSMutableData *data = [NSMutableData data];
    [data appendData:originalURLData(self)];
    [data appendBytes:"\0" length:1];
    return (const char *)[data bytes];
 }

- (NSURL *)_webkit_canonicalize
{
    return URLByCanonicalizingURL(self);
}

- (NSURL *)_web_URLByTruncatingOneCharacterBeforeComponent:(CFURLComponentType)component
{
    return URLByTruncatingOneCharacterBeforeComponent(self, component);
}

- (NSURL *)_webkit_URLByRemovingFragment
{
    return URLByTruncatingOneCharacterBeforeComponent(self, kCFURLComponentFragment);
}

- (NSURL *)_webkit_URLByRemovingResourceSpecifier
{
    return URLByTruncatingOneCharacterBeforeComponent(self, kCFURLComponentResourceSpecifier);
}

- (NSURL *)_web_URLByRemovingUserInfo
{
    return URLByRemovingUserInfo(self);
}

- (BOOL)_webkit_isJavaScriptURL
{
    return [[self _web_originalDataAsString] _webkit_isJavaScriptURL];
}

- (NSString *)_webkit_scriptIfJavaScriptURL
{
    return [[self absoluteString] _webkit_scriptIfJavaScriptURL];
}

- (BOOL)_webkit_isFileURL
{    
    return [[self _web_originalDataAsString] _webkit_isFileURL];
}

- (BOOL)_webkit_isFTPDirectoryURL
{
    return [[self _web_originalDataAsString] _webkit_isFTPDirectoryURL];
}

- (BOOL)_webkit_shouldLoadAsEmptyDocument
{
    return [[self _web_originalDataAsString] _webkit_hasCaseInsensitivePrefix:@"about:"] || [self _web_isEmpty];
}

- (NSURL *)_web_URLWithLowercasedScheme
{
    CFRange range;
    CFURLGetByteRangeForComponent((CFURLRef)self, kCFURLComponentScheme, &range);
    if (range.location == kCFNotFound) {
        return self;
    }
    
    UInt8 static_buffer[URL_BYTES_BUFFER_LENGTH];
    UInt8 *buffer = static_buffer;
    CFIndex bytesFilled = CFURLGetBytes((CFURLRef)self, buffer, URL_BYTES_BUFFER_LENGTH);
    if (bytesFilled == -1) {
        CFIndex bytesToAllocate = CFURLGetBytes((CFURLRef)self, NULL, 0);
        buffer = static_cast<UInt8 *>(malloc(bytesToAllocate));
        bytesFilled = CFURLGetBytes((CFURLRef)self, buffer, bytesToAllocate);
        ASSERT(bytesFilled == bytesToAllocate);
    }
    
    int i;
    BOOL changed = NO;
    for (i = 0; i < range.length; ++i) {
        char c = buffer[range.location + i];
        char lower = toASCIILower(c);
        if (c != lower) {
            buffer[range.location + i] = lower;
            changed = YES;
        }
    }
    
    NSURL *result = changed
        ? CFBridgingRelease(CFURLCreateAbsoluteURLWithBytes(NULL, buffer, bytesFilled, kCFStringEncodingUTF8, nil, YES))
        : self;

    if (buffer != static_buffer) {
        free(buffer);
    }
    
    return result;
}


-(NSData *)_web_schemeSeparatorWithoutColon
{
    NSData *result = nil;
    CFRange rangeWithSeparators;
    CFRange range = CFURLGetByteRangeForComponent((CFURLRef)self, kCFURLComponentScheme, &rangeWithSeparators);
    if (rangeWithSeparators.location != kCFNotFound) {
        NSString *absoluteString = [self absoluteString];
        NSRange separatorsRange = NSMakeRange(range.location + range.length + 1, rangeWithSeparators.length - range.length - 1);
        if (separatorsRange.location + separatorsRange.length <= [absoluteString length]) {
            NSString *slashes = [absoluteString substringWithRange:separatorsRange];
            result = [slashes dataUsingEncoding:NSISOLatin1StringEncoding];
        }
    }
    return result;
}

-(NSData *)_web_dataForURLComponentType:(CFURLComponentType)componentType
{
    return dataForURLComponentType(self, componentType);
}

-(NSData *)_web_schemeData
{
    return dataForURLComponentType(self, kCFURLComponentScheme);
}

-(NSData *)_web_hostData
{
    NSData *result = dataForURLComponentType(self, kCFURLComponentHost);
    NSData *scheme = [self _web_schemeData];
    // Take off localhost for file
    if ([scheme _web_isCaseInsensitiveEqualToCString:"file"]) {
        return ([result _web_isCaseInsensitiveEqualToCString:"localhost"]) ? nil : result;
    }
    return result;
}

- (NSString *)_web_hostString
{
    return [[[NSString alloc] initWithData:[self _web_hostData] encoding:NSUTF8StringEncoding] autorelease];
}

- (NSString *)_webkit_suggestedFilenameWithMIMEType:(NSString *)MIMEType
{
    return suggestedFilenameWithMIMEType(self, MIMEType);
}

- (NSURL *)_webkit_URLFromURLOrSchemelessFileURL
{
    if ([self scheme])
        return self;

    return [NSURL URLWithString:[@"file:" stringByAppendingString:[self absoluteString]]];
}

#if PLATFORM(IOS)
static inline NSURL *createYouTubeURL(NSString *videoID, NSString *timeID)
{
    // This method creates a youtube: URL, which is an internal format that is passed to YouTube.app.
    // If the format of these internal links is changed below, then the app probably needs to be updated as well.

    ASSERT(videoID && [videoID length] > 0 && ![videoID isEqualToString:@"/"]);

    if ([timeID length])
        return [NSURL URLWithString:[NSString stringWithFormat:@"youtube:%@?t=%@", videoID, timeID]];
    return [NSURL URLWithString:[NSString stringWithFormat:@"youtube:%@", videoID]];
}

- (NSURL *)_webkit_youTubeURL
{
    // Bail out early if we aren't even on www.youtube.com or youtube.com.
    NSString *scheme = [[self scheme] lowercaseString];
    if (![scheme isEqualToString:@"http"] && ![scheme isEqualToString:@"https"])
        return nil;

    NSString *hostName = [[self host] lowercaseString];
    BOOL isYouTubeMobileWebAppURL = [hostName isEqualToString:@"m.youtube.com"];
    BOOL isYouTubeShortenedURL = [hostName isEqualToString:@"youtu.be"];
    if (!isYouTubeMobileWebAppURL
        && !isYouTubeShortenedURL
        && ![hostName isEqualToString:@"www.youtube.com"]
        && ![hostName isEqualToString:@"youtube.com"])
        return nil;

    NSString *path = [self path];

    // Short URL of the form: http://youtu.be/v1d301D
    if (isYouTubeShortenedURL) {
        NSString *videoID = [path lastPathComponent];
        if (videoID && ![videoID isEqualToString:@"/"])
            return createYouTubeURL(videoID, nil);
        return nil;
    }

    NSString *query = [self query];
    NSString *fragment = [self fragment];

    // On the YouTube mobile web app, the path and query string are put into the
    // fragment so that one web page is only ever loaded (see <rdar://problem/9550639>).
    if (isYouTubeMobileWebAppURL) {
        NSRange range = [fragment rangeOfString:@"?"];
        if (range.location == NSNotFound) {
            path = fragment;
            query = nil;
        } else {
            path = [fragment substringToIndex:range.location];
            query = [fragment substringFromIndex:(range.location + 1)];
        }
        fragment = nil;
    }

    if ([[path lowercaseString] isEqualToString:@"/watch"]) {
        if ([query length]) {
            NSString *videoID = [[query _webkit_queryKeysAndValues] objectForKey:@"v"];
            if (videoID) {
                NSDictionary *fragmentDict = [[self fragment] _webkit_queryKeysAndValues];
                NSString *timeID = [fragmentDict objectForKey:@"t"];
                return createYouTubeURL(videoID, timeID);
            }
        }

        // May be a new-style link (see <rdar://problem/7733692>).
        if ([fragment hasPrefix:@"!"]) {
            query = [fragment substringFromIndex:1];

            if ([query length]) {
                NSDictionary *queryDict = [query _webkit_queryKeysAndValues];
                NSString *videoID = [queryDict objectForKey:@"v"];
                if (videoID) {
                    NSString *timeID = [queryDict objectForKey:@"t"];
                    return createYouTubeURL(videoID, timeID);
                }
            }
        }
    } else if ([path _web_hasCaseInsensitivePrefix:@"/v/"] || [path _web_hasCaseInsensitivePrefix:@"/e/"]) {
        NSString *videoID = [path lastPathComponent];

        // These URLs are funny - they don't have a ? for the first query parameter.
        // Strip all characters after and including '&' to remove extraneous parameters after the video ID.
        NSRange ampersand = [videoID rangeOfString:@"&"];
        if (ampersand.location != NSNotFound)
            videoID = [videoID substringToIndex:ampersand.location];

        if (videoID)
            return createYouTubeURL(videoID, nil);
    }

    return nil;
}

+ (NSURL *)uniqueURLWithRelativePart:(NSString *)relativePart
{
    CFUUIDRef UUIDRef = CFUUIDCreate(kCFAllocatorDefault);
    NSString *UUIDString = (NSString *)CFUUIDCreateString(kCFAllocatorDefault, UUIDRef);
    CFRelease(UUIDRef);
    NSURL *URL = [NSURL URLWithString:[NSString stringWithFormat:@"%@://%@/%@", WebDataProtocolScheme, UUIDString, relativePart]];
    CFRelease(UUIDString);
    
    return URL;
}

#endif // PLATFORM(IOS)
@end

@implementation NSString (WebNSURLExtras)

- (BOOL)_web_isUserVisibleURL
{
    return isUserVisibleURL(self);
}

- (BOOL)_webkit_isJavaScriptURL
{
    return [self _webkit_hasCaseInsensitivePrefix:@"javascript:"];
}

- (BOOL)_webkit_isFileURL
{
    return [self rangeOfString:@"file:" options:(NSCaseInsensitiveSearch | NSAnchoredSearch)].location != NSNotFound;
}

- (NSString *)_webkit_stringByReplacingValidPercentEscapes
{
    return decodeURLEscapeSequences(self);
}

- (NSString *)_webkit_scriptIfJavaScriptURL
{
    if (![self _webkit_isJavaScriptURL]) {
        return nil;
    }
    return [[self substringFromIndex:11] _webkit_stringByReplacingValidPercentEscapes];
}

- (BOOL)_webkit_isFTPDirectoryURL
{
    int length = [self length];
    if (length < 5) {  // 5 is length of "ftp:/"
        return NO;
    }
    unichar lastChar = [self characterAtIndex:length - 1];
    return lastChar == '/' && [self _webkit_hasCaseInsensitivePrefix:@"ftp:"];
}

- (BOOL)_web_hostNameNeedsDecodingWithRange:(NSRange)range
{
    return hostNameNeedsDecodingWithRange(self, range);
}

- (BOOL)_web_hostNameNeedsEncodingWithRange:(NSRange)range
{
    return hostNameNeedsEncodingWithRange(self, range);
}

- (NSString *)_web_decodeHostNameWithRange:(NSRange)range
{
    return decodeHostNameWithRange(self, range);
}

- (NSString *)_web_encodeHostNameWithRange:(NSRange)range
{
    return encodeHostNameWithRange(self, range);
}

- (NSString *)_web_decodeHostName
{
    return decodeHostName(self);
}

- (NSString *)_web_encodeHostName
{
    return encodeHostName(self);
}

-(NSRange)_webkit_rangeOfURLScheme
{
    NSRange colon = [self rangeOfString:@":"];
    if (colon.location != NSNotFound && colon.location > 0) {
        NSRange scheme = {0, colon.location};
        static NSCharacterSet *InverseSchemeCharacterSet = nil;
        if (!InverseSchemeCharacterSet) {
            /*
             This stuff is very expensive.  10-15 msec on a 2x1.2GHz.  If not cached it swamps
             everything else when adding items to the autocomplete DB.  Makes me wonder if we
             even need to enforce the character set here.
            */
            NSString *acceptableCharacters = @"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+.-";
            InverseSchemeCharacterSet = [[[NSCharacterSet characterSetWithCharactersInString:acceptableCharacters] invertedSet] retain];
        }
        NSRange illegals = [self rangeOfCharacterFromSet:InverseSchemeCharacterSet options:0 range:scheme];
        if (illegals.location == NSNotFound)
            return scheme;
    }
    return NSMakeRange(NSNotFound, 0);
}

-(BOOL)_webkit_looksLikeAbsoluteURL
{
    // Trim whitespace because _web_URLWithString allows whitespace.
    return [[self _webkit_stringByTrimmingWhitespace] _webkit_rangeOfURLScheme].location != NSNotFound;
}

- (NSString *)_webkit_URLFragment
{
    NSRange fragmentRange;
    
    fragmentRange = [self rangeOfString:@"#" options:NSLiteralSearch];
    if (fragmentRange.location == NSNotFound)
        return nil;
    return [self substringFromIndex:fragmentRange.location + 1];
}

#if PLATFORM(IOS)

- (NSString *)_webkit_unescapedQueryValue
{
    NSMutableString *string = [[[self stringByReplacingPercentEscapesUsingEncoding:NSUTF8StringEncoding] mutableCopy] autorelease];
    if (!string) // If we failed to decode the URL as UTF8, fall back to Latin1
        string = [[[self stringByReplacingPercentEscapesUsingEncoding:NSISOLatin1StringEncoding] mutableCopy] autorelease];
    [string replaceOccurrencesOfString:@"+" withString:@" " options:NSLiteralSearch range:NSMakeRange(0, [string length])];
    return string;
}

- (NSDictionary *)_webkit_queryKeysAndValues
{
    unsigned queryLength = [self length];
    if (!queryLength)
        return nil;
    
    NSMutableDictionary *queryKeysAndValues = nil;
    NSRange equalSearchRange = NSMakeRange(0, queryLength);
    
    while (equalSearchRange.location < queryLength - 1 && equalSearchRange.length) {
        
        // Search for "=".
        NSRange equalRange = [self rangeOfString:@"=" options:NSLiteralSearch range:equalSearchRange];
        if (equalRange.location == NSNotFound)
            break;
        
        unsigned indexAfterEqual = equalRange.location + 1;
        if (indexAfterEqual > queryLength - 1)
            break;
                        
        // Get the key before the "=".
        NSRange keyRange = NSMakeRange(equalSearchRange.location, equalRange.location - equalSearchRange.location);
        
        // Seach for the ampersand.
        NSRange ampersandSearchRange = NSMakeRange(indexAfterEqual, queryLength - indexAfterEqual);
        NSRange ampersandRange = [self rangeOfString:@"&" options:NSLiteralSearch range:ampersandSearchRange];
        
        // Get the value after the "=", before the ampersand.
        NSRange valueRange;
        if (ampersandRange.location != NSNotFound)
            valueRange = NSMakeRange(indexAfterEqual, ampersandRange.location - indexAfterEqual);
        else 
            valueRange = NSMakeRange(indexAfterEqual, queryLength - indexAfterEqual);
                
        // Save the key and the value.
        if (keyRange.length && valueRange.length) {
            if (queryKeysAndValues == nil)
                queryKeysAndValues = [NSMutableDictionary dictionary];
            NSString *key = [[self substringWithRange:keyRange] lowercaseString];
            NSString *value = [[self substringWithRange:valueRange] _webkit_unescapedQueryValue];
            if ([key length] && [value length])
                [queryKeysAndValues setObject:value forKey:key];
        }
        
        // At the end.
        if (ampersandRange.location == NSNotFound)
            break;
        
        // Continue searching after the ampersand.
        unsigned indexAfterAmpersand = ampersandRange.location + 1;
        equalSearchRange = NSMakeRange(indexAfterAmpersand, queryLength - indexAfterAmpersand);
    }
    
    return queryKeysAndValues;
}

#endif // PLATFORM(IOS)

@end
