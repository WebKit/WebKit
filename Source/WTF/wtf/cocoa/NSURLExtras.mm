/*
 * Copyright (C) 2005, 2007, 2014 Apple Inc. All rights reserved.
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

#import "config.h"
#import "NSURLExtras.h"

#import <mutex>
#import <wtf/Function.h>
#import <wtf/RetainPtr.h>
#import <wtf/URLHelpers.h>
#import <wtf/URLParser.h>
#import <wtf/Vector.h>
#import <wtf/cf/CFURLExtras.h>

namespace WTF {

using namespace URLHelpers;

constexpr unsigned urlBytesBufferLength = 2048;

static BOOL readIDNAllowedScriptListFile(NSString *filename)
{
    if (!filename)
        return NO;

    FILE *file = fopen([filename fileSystemRepresentation], "r");
    if (!file)
        return NO;
    
    // Read a word at a time.
    // Allow comments, starting with # character to the end of the line.
    while (1) {
        // Skip a comment if present.
        if (fscanf(file, " #%*[^\n\r]%*[\n\r]") == EOF)
            break;
        
        // Read a script name if present.
        char word[33];
        int result = fscanf(file, " %32[^# \t\n\r]%*[^# \t\n\r] ", word);
        if (result == EOF)
            break;
        
        if (result == 1) {
            // Got a word, map to script code and put it into the array.
            addScriptToIDNAllowedScriptList(word);
        }
    }
    fclose(file);
    return YES;
}

namespace URLHelpers {

void loadIDNAllowedScriptList()
{
    static dispatch_once_t flag;
    dispatch_once(&flag, ^{
        for (NSString *directory in NSSearchPathForDirectoriesInDomains(NSLibraryDirectory, NSAllDomainsMask, YES)) {
            if (readIDNAllowedScriptListFile([directory stringByAppendingPathComponent:@"IDNScriptWhiteList.txt"]))
                return;
        }
        initializeDefaultIDNAllowedScriptList();
    });
}

} // namespace URLHelpers
    
static String decodePercentEscapes(const String& string)
{
    NSString *substring = CFBridgingRelease(CFURLCreateStringByReplacingPercentEscapes(nullptr, string.createCFString().get(), CFSTR("")));
    if (!substring)
        return string;
    return substring;
}

NSString *decodeHostName(NSString *string)
{
    Optional<String> host = mapHostName(string, nullptr);
    if (!host)
        return nil;
    return !*host ? string : (NSString *)*host;
}

NSString *encodeHostName(NSString *string)
{
    Optional<String> host = mapHostName(string, decodePercentEscapes);
    if (!host)
        return nil;
    return !*host ? string : (NSString *)*host;
}

static RetainPtr<NSString> stringByTrimmingWhitespace(NSString *string)
{
    auto trimmed = adoptNS([string mutableCopy]);
    CFStringTrimWhitespace((__bridge CFMutableStringRef)trimmed.get());
    return trimmed;
}

NSURL *URLByTruncatingOneCharacterBeforeComponent(NSURL *URL, CFURLComponentType component)
{
    if (!URL)
        return nil;
    
    CFRange fragRg = CFURLGetByteRangeForComponent((__bridge CFURLRef)URL, component, nullptr);
    if (fragRg.location == kCFNotFound)
        return URL;

    Vector<UInt8, urlBytesBufferLength> urlBytes(urlBytesBufferLength);
    CFIndex numBytes = CFURLGetBytes((__bridge CFURLRef)URL, urlBytes.data(), urlBytes.size());
    if (numBytes == -1) {
        numBytes = CFURLGetBytes((__bridge CFURLRef)URL, nullptr, 0);
        urlBytes.grow(numBytes);
        CFURLGetBytes((__bridge CFURLRef)URL, urlBytes.data(), numBytes);
    }

    CFURLRef result = CFURLCreateWithBytes(nullptr, urlBytes.data(), fragRg.location - 1, kCFStringEncodingUTF8, nullptr);
    if (!result)
        result = CFURLCreateWithBytes(nullptr, urlBytes.data(), fragRg.location - 1, kCFStringEncodingISOLatin1, nullptr);
        
    return result ? CFBridgingRelease(result) : URL;
}

static NSURL *URLByRemovingResourceSpecifier(NSURL *URL)
{
    return URLByTruncatingOneCharacterBeforeComponent(URL, kCFURLComponentResourceSpecifier);
}

NSURL *URLWithData(NSData *data, NSURL *baseURL)
{
    if (!data)
        return nil;
    
    NSURL *result = nil;
    size_t length = [data length];
    if (length > 0) {
        // work around <rdar://4470771>: CFURLCreateAbsoluteURLWithBytes(.., TRUE) doesn't remove non-path components.
        baseURL = URLByRemovingResourceSpecifier(baseURL);
        
        const UInt8 *bytes = static_cast<const UInt8*>([data bytes]);
        
        // CFURLCreateAbsoluteURLWithBytes would complain to console if we passed a path to it.
        if (bytes[0] == '/' && !baseURL)
            return nil;
        
        // NOTE: We use UTF-8 here since this encoding is used when computing strings when returning URL components
        // (e.g calls to NSURL -path). However, this function is not tolerant of illegal UTF-8 sequences, which
        // could either be a malformed string or bytes in a different encoding, like shift-jis, so we fall back
        // onto using ISO Latin 1 in those cases.
        result = CFBridgingRelease(CFURLCreateAbsoluteURLWithBytes(nullptr, bytes, length, kCFStringEncodingUTF8, (__bridge CFURLRef)baseURL, YES));
        if (!result)
            result = CFBridgingRelease(CFURLCreateAbsoluteURLWithBytes(nullptr, bytes, length, kCFStringEncodingISOLatin1, (__bridge CFURLRef)baseURL, YES));
    } else
        result = [NSURL URLWithString:@""];

    return result;
}
static NSData *dataWithUserTypedString(NSString *string)
{
    NSData *userTypedData = [string dataUsingEncoding:NSUTF8StringEncoding];
    ASSERT(userTypedData);
    
    const UInt8* inBytes = static_cast<const UInt8 *>([userTypedData bytes]);
    int inLength = [userTypedData length];
    if (!inLength)
        return nil;

    Checked<int, RecordOverflow> mallocLength = inLength;
    mallocLength *= 3; // large enough to %-escape every character
    if (mallocLength.hasOverflowed())
        return nil;
    
    char* outBytes = static_cast<char *>(malloc(mallocLength.unsafeGet()));
    char* p = outBytes;
    int outLength = 0;
    for (int i = 0; i < inLength; i++) {
        UInt8 c = inBytes[i];
        if (c <= 0x20 || c >= 0x7f) {
            *p++ = '%';
            *p++ = upperNibbleToASCIIHexDigit(c);
            *p++ = lowerNibbleToASCIIHexDigit(c);
            outLength += 3;
        } else {
            *p++ = c;
            outLength++;
        }
    }
    
    return [NSData dataWithBytesNoCopy:outBytes length:outLength]; // adopts outBytes
}

NSURL *URLWithUserTypedString(NSString *string, NSURL *nsURL)
{
    if (!string)
        return nil;

    auto mappedString = mapHostNames(stringByTrimmingWhitespace(string).get(), decodePercentEscapes);
    if (!mappedString)
        return nil;

    // Let's check whether the URL is bogus.
    URL url { URL { nsURL }, mappedString };
    if (!url.createCFURL())
        return nil;

    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=186057
    // We should be able to use url.createCFURL instead of using directly CFURL parsing routines.
    NSData *data = dataWithUserTypedString(mappedString);
    if (!data)
        return [NSURL URLWithString:@""];

    return URLWithData(data, nsURL);
}

NSURL *URLWithUserTypedStringDeprecated(NSString *string, NSURL *URL)
{
    if (!string)
        return nil;

    NSURL *result = URLWithUserTypedString(string, URL);
    if (!result) {
        NSData *resultData = dataWithUserTypedString(string);
        if (!resultData)
            return [NSURL URLWithString:@""];
        result = URLWithData(resultData, URL);
    }

    return result;
}

static BOOL hasQuestionMarkOnlyQueryString(NSURL *URL)
{
    CFRange rangeWithSeparators;
    CFURLGetByteRangeForComponent((__bridge CFURLRef)URL, kCFURLComponentQuery, &rangeWithSeparators);
    if (rangeWithSeparators.location != kCFNotFound && rangeWithSeparators.length == 1)
        return YES;

    return NO;
}

NSData *dataForURLComponentType(NSURL *URL, CFURLComponentType componentType)
{
    Vector<UInt8, urlBytesBufferLength> allBytesBuffer(urlBytesBufferLength);
    CFIndex bytesFilled = CFURLGetBytes((__bridge CFURLRef)URL, allBytesBuffer.data(), allBytesBuffer.size());
    if (bytesFilled == -1) {
        CFIndex bytesToAllocate = CFURLGetBytes((__bridge CFURLRef)URL, nullptr, 0);
        allBytesBuffer.grow(bytesToAllocate);
        bytesFilled = CFURLGetBytes((__bridge CFURLRef)URL, allBytesBuffer.data(), bytesToAllocate);
    }
    
    const CFURLComponentType completeURL = (CFURLComponentType)-1;
    CFRange range;
    if (componentType != completeURL) {
        range = CFURLGetByteRangeForComponent((__bridge CFURLRef)URL, componentType, nullptr);
        if (range.location == kCFNotFound)
            return nil;
    } else {
        range.location = 0;
        range.length = bytesFilled;
    }
    
    NSData *componentData = [NSData dataWithBytes:allBytesBuffer.data() + range.location length:range.length]; 
    
    const unsigned char *bytes = static_cast<const unsigned char *>([componentData bytes]);
    NSMutableData *resultData = [NSMutableData data];
    // NOTE: add leading '?' to query strings non-zero length query strings.
    // NOTE: retain question-mark only query strings.
    if (componentType == kCFURLComponentQuery) {
        if (range.length > 0 || hasQuestionMarkOnlyQueryString(URL))
            [resultData appendBytes:"?" length:1];    
    }
    for (int i = 0; i < range.length; i++) {
        unsigned char c = bytes[i];
        if (c <= 0x20 || c >= 0x7f) {
            char escaped[3];
            escaped[0] = '%';
            escaped[1] = upperNibbleToASCIIHexDigit(c);
            escaped[2] = lowerNibbleToASCIIHexDigit(c);
            [resultData appendBytes:escaped length:3];    
        } else {
            char b[1];
            b[0] = c;
            [resultData appendBytes:b length:1];    
        }               
    }
    
    return resultData;
}

static NSURL *URLByRemovingComponentAndSubsequentCharacter(NSURL *URL, CFURLComponentType component)
{
    CFRange range = CFURLGetByteRangeForComponent((__bridge CFURLRef)URL, component, 0);
    if (range.location == kCFNotFound)
        return URL;
    
    // Remove one subsequent character.
    range.length++;

    Vector<UInt8, urlBytesBufferLength> buffer(urlBytesBufferLength);
    CFIndex numBytes = CFURLGetBytes((__bridge CFURLRef)URL, buffer.data(), buffer.size());
    if (numBytes == -1) {
        numBytes = CFURLGetBytes((__bridge CFURLRef)URL, nullptr, 0);
        buffer.grow(numBytes);
        CFURLGetBytes((__bridge CFURLRef)URL, buffer.data(), numBytes);
    }
    UInt8* urlBytes = buffer.data();
        
    if (numBytes < range.location)
        return URL;
    if (numBytes < range.location + range.length)
        range.length = numBytes - range.location;
        
    memmove(urlBytes + range.location, urlBytes + range.location + range.length, numBytes - range.location + range.length);
    
    CFURLRef result = CFURLCreateWithBytes(nullptr, urlBytes, numBytes - range.length, kCFStringEncodingUTF8, nullptr);
    if (!result)
        result = CFURLCreateWithBytes(nullptr, urlBytes, numBytes - range.length, kCFStringEncodingISOLatin1, nullptr);
                
    return result ? CFBridgingRelease(result) : URL;
}

NSURL *URLByRemovingUserInfo(NSURL *URL)
{
    return URLByRemovingComponentAndSubsequentCharacter(URL, kCFURLComponentUserInfo);
}

NSData *originalURLData(NSURL *URL)
{
    UInt8 *buffer = (UInt8 *)malloc(urlBytesBufferLength);
    CFIndex bytesFilled = CFURLGetBytes((__bridge CFURLRef)URL, buffer, urlBytesBufferLength);
    if (bytesFilled == -1) {
        CFIndex bytesToAllocate = CFURLGetBytes((__bridge CFURLRef)URL, nullptr, 0);
        buffer = (UInt8 *)realloc(buffer, bytesToAllocate);
        bytesFilled = CFURLGetBytes((__bridge CFURLRef)URL, buffer, bytesToAllocate);
        ASSERT(bytesFilled == bytesToAllocate);
    }
    
    // buffer is adopted by the NSData
    NSData *data = [NSData dataWithBytesNoCopy:buffer length:bytesFilled freeWhenDone:YES];
    
    NSURL *baseURL = (__bridge NSURL *)CFURLGetBaseURL((__bridge CFURLRef)URL);
    if (baseURL)
        return originalURLData(URLWithData(data, baseURL));
    return data;
}

NSString *userVisibleString(NSURL *URL)
{
    NSData *data = originalURLData(URL);
    CString string(static_cast<const char*>([data bytes]), [data length]);
    return userVisibleURL(string);
}

BOOL isUserVisibleURL(NSString *string)
{
    BOOL valid = YES;
    // get buffer
    
    char static_buffer[1024];
    const char *p;
    BOOL success = CFStringGetCString((__bridge CFStringRef)string, static_buffer, 1023, kCFStringEncodingUTF8);
    p = success ? static_buffer : [string UTF8String];
    
    int length = strlen(p);
    
    // check for characters <= 0x20 or >=0x7f, %-escape sequences of %7f, and xn--, these
    // are the things that will lead _web_userVisibleString to actually change things.
    for (int i = 0; i < length; i++) {
        unsigned char c = p[i];
        // escape control characters, space, and delete
        if (c <= 0x20 || c == 0x7f) {
            valid = NO;
            break;
        } else if (c == '%' && (i + 1 < length && isASCIIHexDigit(p[i + 1])) && i + 2 < length && isASCIIHexDigit(p[i + 2])) {
            auto u = toASCIIHexValue(p[i + 1], p[i + 2]);
            if (u > 0x7f) {
                valid = NO;
                break;
            }
            i += 2;
        } else {
            // Check for "xn--" in an efficient, non-case-sensitive, way.
            if (c == '-' && i >= 3 && (p[i - 3] | 0x20) == 'x' && (p[i - 2] | 0x20) == 'n' && p[i - 1] == '-') {
                valid = NO;
                break;
            }
        }
    }
    
    return valid;
}

NSRange rangeOfURLScheme(NSString *string)
{
    NSRange colon = [string rangeOfString:@":"];
    if (colon.location != NSNotFound && colon.location > 0) {
        NSRange scheme = {0, colon.location};
        /*
         This stuff is very expensive.  10-15 msec on a 2x1.2GHz.  If not cached it swamps
         everything else when adding items to the autocomplete DB.  Makes me wonder if we
         even need to enforce the character set here.
         */
        NSString *acceptableCharacters = @"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+.-";
        static LazyNeverDestroyed<RetainPtr<NSCharacterSet>> inverseSchemeCharacterSet;
        static std::once_flag onceKey;
        std::call_once(onceKey, [&] {
            inverseSchemeCharacterSet.construct([[NSCharacterSet characterSetWithCharactersInString:acceptableCharacters] invertedSet]);
        });
        NSRange illegals = [string rangeOfCharacterFromSet:inverseSchemeCharacterSet.get().get() options:0 range:scheme];
        if (illegals.location == NSNotFound)
            return scheme;
    }
    return NSMakeRange(NSNotFound, 0);
}

BOOL looksLikeAbsoluteURL(NSString *string)
{
    // Trim whitespace because _web_URLWithString allows whitespace.
    return rangeOfURLScheme(stringByTrimmingWhitespace(string).get()).location != NSNotFound;
}

} // namespace WebCore
