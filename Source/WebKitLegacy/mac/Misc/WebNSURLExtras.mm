/*
 * Copyright (C) 2005-2021 Apple Inc. All rights reserved.
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
#import <WebCore/WebCoreNSURLExtras.h>
#import <pal/text/TextEncoding.h>
#import <unicode/uchar.h>
#import <unicode/uscript.h>
#import <wtf/Assertions.h>
#import <wtf/URL.h>
#import <wtf/cocoa/NSURLExtras.h>
#import <wtf/cocoa/TypeCastsCocoa.h>

@implementation NSURL (WebNSURLExtras)

+ (NSURL *)_web_URLWithUserTypedString:(NSString *)string
{
    return WTF::URLWithUserTypedStringDeprecated(string);
}

+ (NSURL *)_webkit_URLWithUserTypedString:(NSString *)string
{
    return WTF::URLWithUserTypedString(string);
}

+ (NSURL *)_web_URLWithDataAsString:(NSString *)string
{
    return [self _web_URLWithDataAsString:string relativeToURL:nil];
}

+ (NSURL *)_web_URLWithDataAsString:(NSString *)string relativeToURL:(NSURL *)baseURL
{
    return WTF::URLWithData([[string _webkit_stringByTrimmingWhitespace] dataUsingEncoding:NSISOLatin1StringEncoding], baseURL);
}

- (NSData *)_web_originalData
{
    return WTF::originalURLData(self);
}

- (NSString *)_web_originalDataAsString
{
    return adoptNS([[NSString alloc] initWithData:WTF::originalURLData(self) encoding:NSISOLatin1StringEncoding]).autorelease();
}

- (NSString *)_web_userVisibleString
{
    return WTF::userVisibleString(self);
}

- (BOOL)_web_isEmpty
{
    if (!CFURLGetBaseURL(bridge_cast(self)))
        return !CFURLGetBytes(bridge_cast(self), nullptr, 0);
    return ![WTF::originalURLData(self) length];
}

- (const char*)_web_URLCString
{
    NSMutableData *data = [NSMutableData data];
    [data appendData:WTF::originalURLData(self)];
    [data appendBytes:"\0" length:1];
    return (const char*)[data bytes];
 }

- (NSURL *)_webkit_canonicalize
{
    return WebCore::URLByCanonicalizingURL(self);
}

- (NSURL *)_webkit_canonicalize_with_wtf
{
    auto url = WTF::URL(self);
    return url.isValid() ? (NSURL *)url : nil;
}

- (NSURL *)_webkit_URLByRemovingFragment 
{
    return WTF::URLByTruncatingOneCharacterBeforeComponent(self, kCFURLComponentFragment);
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

-(NSData *)_web_schemeData
{
    return WTF::dataForURLComponentType(self, kCFURLComponentScheme);
}

-(NSData *)_web_hostData
{
    NSData *result = WTF::dataForURLComponentType(self, kCFURLComponentHost);

    // Take off localhost for file.
    if ([result _web_isCaseInsensitiveEqualToCString:"localhost"] && [[self _web_schemeData] _web_isCaseInsensitiveEqualToCString:"file"])
        return nil;

    return result;
}

- (NSString *)_web_hostString
{
    return adoptNS([[NSString alloc] initWithData:[self _web_hostData] encoding:NSUTF8StringEncoding]).autorelease();
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
    return WTF::isUserVisibleURL(self);
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
    return PAL::decodeURLEscapeSequences(String(self));
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
    NSString *name = WTF::decodeHostName(self);
    return !name ? self : name;
}

- (NSString *)_web_encodeHostName
{
    NSString *name = WTF::encodeHostName(self);
    return !name ? self : name;
}

- (NSString *)_webkit_decodeHostName
{
    return WTF::decodeHostName(self);
}

- (NSString *)_webkit_encodeHostName
{
    return WTF::encodeHostName(self);
}

-(NSRange)_webkit_rangeOfURLScheme
{
    NSRange colon = [self rangeOfString:@":"];
    if (colon.location != NSNotFound && colon.location > 0) {
        NSRange scheme = {0, colon.location};
        static NeverDestroyed inverseSchemeCharacterSet = [] {
            /*
             This stuff is very expensive.  10-15 msec on a 2x1.2GHz.  If not cached it swamps
             everything else when adding items to the autocomplete DB.  Makes me wonder if we
             even need to enforce the character set here.
            */
            NSString *acceptableCharacters = @"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+.-";
            return RetainPtr { [[NSCharacterSet characterSetWithCharactersInString:acceptableCharacters] invertedSet] };
        }();
        NSRange illegals = [self rangeOfCharacterFromSet:inverseSchemeCharacterSet.get().get() options:0 range:scheme];
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
