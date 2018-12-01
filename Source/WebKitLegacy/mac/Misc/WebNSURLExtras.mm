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
#import <Foundation/NSURLRequest.h>
#import <WebCore/LoaderNSURLExtras.h>
#import <WebCore/TextEncoding.h>
#import <WebCore/WebCoreNSURLExtras.h>
#import <unicode/uchar.h>
#import <unicode/uscript.h>
#import <wtf/Assertions.h>
#import <wtf/ObjCRuntimeExtras.h>
#import <wtf/URL.h>
#import <wtf/cocoa/NSURLExtras.h>

using namespace WebCore;
using namespace WTF;

#define URL_BYTES_BUFFER_LENGTH 2048

@implementation NSURL (WebNSURLExtras)

+ (NSURL *)_web_URLWithUserTypedString:(NSString *)string relativeToURL:(NSURL *)URL
{
    return URLWithUserTypedStringDeprecated(string, URL);
}

+ (NSURL *)_web_URLWithUserTypedString:(NSString *)string
{
    return URLWithUserTypedStringDeprecated(string, nil);
}

+ (NSURL *)_webkit_URLWithUserTypedString:(NSString *)string relativeToURL:(NSURL *)URL
{
    return URLWithUserTypedString(string, URL);
}

+ (NSURL *)_webkit_URLWithUserTypedString:(NSString *)string
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
    return WTF::userVisibleString(self);
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

- (NSURL *)_webkit_URLByRemovingFragment 
{
    return URLByTruncatingOneCharacterBeforeComponent(self, kCFURLComponentFragment);
}

- (NSURL *)_web_URLByRemovingUserInfo
{
    return WTF::URLByRemovingUserInfo(self);
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
    return WTF::dataForURLComponentType(self, componentType);
}

-(NSData *)_web_schemeData
{
    return WTF::dataForURLComponentType(self, kCFURLComponentScheme);
}

-(NSData *)_web_hostData
{
    NSData *result = WTF::dataForURLComponentType(self, kCFURLComponentHost);
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

- (NSString *)_web_decodeHostName
{
    NSString *name = decodeHostName(self);
    return !name ? self : name;
}

- (NSString *)_web_encodeHostName
{
    NSString *name = encodeHostName(self);
    return !name ? self : name;
}

- (NSString *)_webkit_decodeHostName
{
    return decodeHostName(self);
}

- (NSString *)_webkit_encodeHostName
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

@end
