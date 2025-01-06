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
#import <wtf/StdLibExtras.h>
#import <wtf/URL.h>
#import <wtf/URLHelpers.h>
#import <wtf/Vector.h>
#import <wtf/cf/CFURLExtras.h>
#import <wtf/cocoa/SpanCocoa.h>
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

NSURL *URLWithData(NSData *data, NSURL *baseURL)
{
    if (!data)
        return nil;
    
    size_t length = [data length];
    if (length > 0) {
        // Work around <rdar://4470771>: CFURLCreateAbsoluteURLWithBytes(.., TRUE) doesn't remove non-path components.
        baseURL = URLByTruncatingOneCharacterBeforeComponent(baseURL, kCFURLComponentResourceSpecifier);

        const UInt8 *bytes = static_cast<const UInt8*>([data bytes]);

        // CFURLCreateAbsoluteURLWithBytes would complain to console if we passed a path to it.
        if (bytes[0] == '/' && !baseURL)
            return nil;

        // NOTE: We use UTF-8 here since this encoding is used when computing strings when returning URL components
        // (e.g calls to NSURL -path). However, this function is not tolerant of illegal UTF-8 sequences, which
        // could either be a malformed string or bytes in a different encoding, like shift-jis, so we fall back
        // onto using ISO Latin 1 in those cases.
        auto result = adoptCF(CFURLCreateAbsoluteURLWithBytes(nullptr, bytes, length, kCFStringEncodingUTF8, (__bridge CFURLRef)baseURL, YES));
        if (!result)
            result = adoptCF(CFURLCreateAbsoluteURLWithBytes(nullptr, bytes, length, kCFStringEncodingISOLatin1, (__bridge CFURLRef)baseURL, YES));
        return result.bridgingAutorelease();
    }
    return [NSURL URLWithString:@""];
}

static RetainPtr<NSData> dataWithUserTypedString(NSString *string)
{
    RetainPtr userTypedData = [string dataUsingEncoding:NSUTF8StringEncoding];
    ASSERT(userTypedData);
    
    auto inBytes = span(userTypedData.get());
    if (inBytes.empty())
        return nil;

    CheckedInt32 outBytesCapacity = inBytes.size();
    outBytesCapacity *= 3; // large enough to %-escape every character
    if (outBytesCapacity.hasOverflowed())
        return nil;
    
    Vector<char> outBytes;
    outBytes.reserveInitialCapacity(outBytesCapacity);
    for (auto c : inBytes) {
        if (c <= 0x20 || c >= 0x7f)
            outBytes.appendList({ '%', upperNibbleToASCIIHexDigit(c), lowerNibbleToASCIIHexDigit(c) });
        else
            outBytes.append(c);
    }
    
    auto outBytesSpan = outBytes.releaseBuffer().leakSpan();
    return adoptNS([[NSData alloc] initWithBytesNoCopy:outBytesSpan.data() length:outBytesSpan.size() deallocator:^(void* bytes, NSUInteger) {
        FastMalloc::free(bytes);
    }]); // adopts outBytes
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
    RetainPtr data = dataWithUserTypedString(mappedString);
    if (!data)
        return [NSURL URLWithString:@""];

    return URLWithData(data.get(), nil);
}

NSURL *URLWithUserTypedStringDeprecated(NSString *string)
{
    if (!string)
        return nil;

    NSURL *result = URLWithUserTypedString(string);
    if (!result) {
        RetainPtr resultData = dataWithUserTypedString(string);
        if (!resultData)
            return [NSURL URLWithString:@""];
        result = URLWithData(resultData.get(), nil);
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
    auto bytes = bytesBuffer.subspan(range.location);

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
    auto urlBytes = bytes.mutableSpan();
    CFIndex numBytes = bytes.size();

    if (numBytes < range.location)
        return URL;
    if (numBytes < range.location + range.length)
        range.length = numBytes - range.location;

    memmoveSpan(urlBytes.subspan(range.location), urlBytes.subspan(range.location + range.length, numBytes - range.location + range.length));

    auto result = adoptCF(CFURLCreateWithBytes(nullptr, urlBytes.data(), numBytes - range.length, kCFStringEncodingUTF8, nullptr));
    if (!result)
        result = adoptCF(CFURLCreateWithBytes(nullptr, urlBytes.data(), numBytes - range.length, kCFStringEncodingISOLatin1, nullptr));

    return result ? result.bridgingAutorelease() : URL;
}

NSURL *URLByRemovingUserInfo(NSURL *URL)
{
    return URLByRemovingComponentAndSubsequentCharacter(URL, kCFURLComponentUserInfo);
}

NSData *originalURLData(NSURL *URL)
{
    auto data = bridge_cast(bytesAsCFData(bridge_cast(URL)));
    if (auto baseURL = bridge_cast(CFURLGetBaseURL(bridge_cast(URL))))
        return originalURLData(URLWithData(data.get(), baseURL));
    return data.autorelease();
}

NSString *userVisibleString(NSURL *URL)
{
    return URLHelpers::userVisibleURL(span(originalURLData(URL)));
}

BOOL isUserVisibleURL(NSString *string)
{
    // Return true if the userVisibleString function is guaranteed to not change the passed-in URL.
    // This function is used to optimize all the most common cases where we don't need the userVisibleString algorithm.

    std::array<char, 1024> buffer;
    auto success = CFStringGetCString(bridge_cast(string), buffer.data(), buffer.size() - 1, kCFStringEncodingUTF8);
    auto characters = success ? span(buffer.data()) : span([string UTF8String]);

    // Check for control characters, %-escape sequences that are non-ASCII, and xn--: these
    // are the things that might lead the userVisibleString function to actually change the string.
    for (auto character : characters) {
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
