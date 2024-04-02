/*
 * Copyright (C) 2005-2021 Apple Inc. All rights reserved.
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
#import <wtf/URL.h>
#import <wtf/URLHelpers.h>
#import <wtf/Vector.h>
#import <wtf/cf/CFURLExtras.h>
#import <wtf/cocoa/TypeCastsCocoa.h>

namespace WTF {

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
            URLHelpers::addScriptToIDNAllowedScriptList(word);
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
    auto substring = adoptCF(CFURLCreateStringByReplacingPercentEscapes(nullptr, string.createCFString().get(), CFSTR("")));
    if (!substring)
        return string;
    return substring.get();
}

NSString *decodeHostName(NSString *string)
{
    std::optional<String> host = URLHelpers::mapHostName(string, nullptr);
    if (!host)
        return nil;
    return !*host ? string : (NSString *)*host;
}

NSString *encodeHostName(NSString *string)
{
    std::optional<String> host = URLHelpers::mapHostName(string, decodePercentEscapes);
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
    
    CFRange range = CFURLGetByteRangeForComponent(bridge_cast(URL), component, nullptr);
    if (range.location == kCFNotFound)
        return URL;

    auto bytes = bytesAsVector(bridge_cast(URL));

    auto result = adoptCF(CFURLCreateWithBytes(nullptr, bytes.data(), range.location - 1, kCFStringEncodingUTF8, nullptr));
    if (!result)
        result = adoptCF(CFURLCreateWithBytes(nullptr, bytes.data(), range.location - 1, kCFStringEncodingISOLatin1, nullptr));

    return result ? result.bridgingAutorelease() : URL;
}

static NSData *dataWithUserTypedString(NSString *string)
{
    NSData *userTypedData = [string dataUsingEncoding:NSUTF8StringEncoding];
    ASSERT(userTypedData);
    
    const UInt8* inBytes = static_cast<const UInt8 *>([userTypedData bytes]);
    NSUInteger inLength = [userTypedData length];

    // large enough to %-escape every character
    if (!inLength || inLength > (INT32_MAX / 3))
        return nil;

    char* outBytes = static_cast<char *>(malloc(inLength * 3U));
    char* p = outBytes;
    NSUInteger outLength = 0;
    for (NSUInteger i = 0; i < inLength; i++) {
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

NSURL *URLWithUserTypedString(NSString *string, NSURL *)
{
    if (!string)
        return nil;

    auto mappedString = URLHelpers::mapHostNames(stringByTrimmingWhitespace(string).get(), decodePercentEscapes);
    if (!mappedString)
        return nil;

    // Let's check whether the URL is bogus.
    URL url { URL { }, mappedString };
    if (!url.createCFURL())
        return nil;

    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=186057
    // We should be able to use url.createCFURL instead of using directly CFURL parsing routines.
    NSData *data = dataWithUserTypedString(mappedString);
    if (!data)
        return [NSURL URLWithString:@""];

    return [NSURL absoluteURLWithDataRepresentation:data relativeToURL:nil];
}

NSURL *URLWithUserTypedStringDeprecated(NSString *string)
{
    if (!string)
        return nil;

    NSURL *result = URLWithUserTypedString(string);
    if (!result) {
        NSData *resultData = dataWithUserTypedString(string);
        if (!resultData)
            return [NSURL URLWithString:@""];
        result = [NSURL absoluteURLWithDataRepresentation:resultData relativeToURL:nil];
    }

    return result;
}

static bool hasQuestionMarkOnlyQueryString(NSURL *URL)
{
    CFRange rangeWithSeparators;
    CFURLGetByteRangeForComponent(bridge_cast(URL), kCFURLComponentQuery, &rangeWithSeparators);
    return rangeWithSeparators.location != kCFNotFound && rangeWithSeparators.length == 1;
}

NSData *dataForURLComponentType(NSURL *URL, CFURLComponentType componentType)
{
    CFRange range = CFURLGetByteRangeForComponent(bridge_cast(URL), componentType, nullptr);
    if (range.location == kCFNotFound)
        return nil;

    auto bytesBuffer = bytesAsVector(bridge_cast(URL));
    auto bytes = bytesBuffer.data() + range.location;

    NSMutableData *result = [NSMutableData data];

    // We add leading '?' to non-zero length query strings including question-mark only query strings.
    if (componentType == kCFURLComponentQuery) {
        if (range.length || hasQuestionMarkOnlyQueryString(URL))
            [result appendBytes:"?" length:1];
    }

    for (CFIndex i = 0; i < range.length; i++) {
        unsigned char c = bytes[i];
        if (c > 0x20 && c < 0x7F)
            [result appendBytes:&bytes[i] length:1];
        else {
            char escaped[3] = { '%', upperNibbleToASCIIHexDigit(c), lowerNibbleToASCIIHexDigit(c) };
            [result appendBytes:escaped length:3];
        }
    }

    return result;
}

static NSURL *URLByRemovingComponentAndSubsequentCharacter(NSURL *URL, CFURLComponentType component)
{
    CFRange range = CFURLGetByteRangeForComponent(bridge_cast(URL), component, 0);
    if (range.location == kCFNotFound)
        return URL;

    // Remove one subsequent character.
    range.length++;

    auto bytes = bytesAsVector(bridge_cast(URL));
    auto urlBytes = bytes.data();
    CFIndex numBytes = bytes.size();

    if (numBytes < range.location)
        return URL;
    if (numBytes < range.location + range.length)
        range.length = numBytes - range.location;

    memmove(urlBytes + range.location, urlBytes + range.location + range.length, numBytes - range.location + range.length);

    auto result = adoptCF(CFURLCreateWithBytes(nullptr, urlBytes, numBytes - range.length, kCFStringEncodingUTF8, nullptr));
    if (!result)
        result = adoptCF(CFURLCreateWithBytes(nullptr, urlBytes, numBytes - range.length, kCFStringEncodingISOLatin1, nullptr));

    return result ? result.bridgingAutorelease() : URL;
}

NSURL *URLByRemovingUserInfo(NSURL *URL)
{
    return URLByRemovingComponentAndSubsequentCharacter(URL, kCFURLComponentUserInfo);
}

NSString *userVisibleString(NSURL *URL)
{
    NSData *data = URL.dataRepresentation;
    return URLHelpers::userVisibleURL(CString(static_cast<const char*>([data bytes]), [data length]));
}

BOOL isUserVisibleURL(NSString *string)
{
    // Return true if the userVisibleString function is guaranteed to not change the passed-in URL.
    // This function is used to optimize all the most common cases where we don't need the userVisibleString algorithm.
    auto characters = string.UTF8String;

    // Check for control characters, %-escape sequences that are non-ASCII, and xn--: these
    // are the things that might lead the userVisibleString function to actually change the string.
    while (auto character = *characters++) {
        // Control characters, including space, will be escaped by userVisibleString.
        if (character <= 0x20 || character == 0x7F)
            return NO;
        // Escape sequences that expand to non-ASCII characters may be converted to non-escaped UTF-8 sequences.
        if (character == '%' && isASCIIHexDigit(characters[0]) && isASCIIHexDigit(characters[1]) && !isASCII(toASCIIHexValue(characters[0], characters[1])))
            return NO;
        // If "xn--" appears, then we might need to run the IDN algorithm if it's a host name.
        if (isASCIIAlphaCaselessEqual(character, 'x') && isASCIIAlphaCaselessEqual(characters[0], 'n') && characters[1] == '-' && characters[2] == '-')
            return NO;
    }

    return YES;
}

}
