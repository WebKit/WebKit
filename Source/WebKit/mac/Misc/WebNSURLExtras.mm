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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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
#import "WebNSObjectExtras.h"
#import "WebSystemInterface.h"
#import <Foundation/NSURLRequest.h>
#import <WebCore/KURL.h>
#import <WebCore/LoaderNSURLExtras.h>
#import <WebKitSystemInterface.h>
#import <wtf/Assertions.h>
#import <unicode/uchar.h>
#import <unicode/uidna.h>
#import <unicode/uscript.h>

using namespace WebCore;
using namespace WTF;

typedef void (* StringRangeApplierFunction)(NSString *string, NSRange range, void *context);

// Needs to be big enough to hold an IDN-encoded name.
// For host names bigger than this, we won't do IDN encoding, which is almost certainly OK.
#define HOST_NAME_BUFFER_LENGTH 2048

#define URL_BYTES_BUFFER_LENGTH 2048

static pthread_once_t IDNScriptWhiteListFileRead = PTHREAD_ONCE_INIT;
static uint32_t IDNScriptWhiteList[(USCRIPT_CODE_LIMIT + 31) / 32];

static inline BOOL isLookalikeCharacter(int charCode)
{
// FIXME: Move this code down into WebCore so it can be shared with other platforms.

// This function treats the following as unsafe, lookalike characters:
// any non-printable character, any character considered as whitespace that isn't already converted to a space by ICU, 
// and any ignorable character.

// We also considered the characters in Mozilla's blacklist (http://kb.mozillazine.org/Network.IDN.blacklist_chars), 
// and included all of these characters that ICU can encode.

    if (!u_isprint(charCode) || u_isUWhiteSpace(charCode) || u_hasBinaryProperty(charCode, UCHAR_DEFAULT_IGNORABLE_CODE_POINT))
        return YES;

    switch (charCode) {
        case 0x00ED: /* LATIN SMALL LETTER I WITH ACUTE */
        case 0x01C3: /* LATIN LETTER RETROFLEX CLICK */
        case 0x0251: /* LATIN SMALL LETTER ALPHA */
        case 0x0261: /* LATIN SMALL LETTER SCRIPT G */
        case 0x0337: /* COMBINING SHORT SOLIDUS OVERLAY */
        case 0x0338: /* COMBINING LONG SOLIDUS OVERLAY */
        case 0x05B4: /* HEBREW POINT HIRIQ */
        case 0x05BC: /* HEBREW POINT DAGESH OR MAPIQ */
        case 0x05C3: /* HEBREW PUNCTUATION SOF PASUQ */
        case 0x05F4: /* HEBREW PUNCTUATION GERSHAYIM */
        case 0x0660: /* ARABIC INDIC DIGIT ZERO */
        case 0x06D4: /* ARABIC FULL STOP */
        case 0x06F0: /* EXTENDED ARABIC INDIC DIGIT ZERO */
        case 0x2027: /* HYPHENATION POINT */
        case 0x2039: /* SINGLE LEFT-POINTING ANGLE QUOTATION MARK */
        case 0x203A: /* SINGLE RIGHT-POINTING ANGLE QUOTATION MARK */
        case 0x2044: /* FRACTION SLASH */
        case 0x2215: /* DIVISION SLASH */
        case 0x2216: /* SET MINUS */
        case 0x233F: /* APL FUNCTIONAL SYMBOL SLASH BAR */
        case 0x23AE: /* INTEGRAL EXTENSION */
        case 0x244A: /* OCR DOUBLE BACKSLASH */
        case 0x2571: /* BOX DRAWINGS LIGHT DIAGONAL UPPER RIGHT TO LOWER LEFT */
        case 0x2572: /* BOX DRAWINGS LIGHT DIAGONAL UPPER LEFT TO LOWER RIGHT */
        case 0x29F8: /* BIG SOLIDUS */
        case 0x29f6: /* SOLIDUS WITH OVERBAR */
        case 0x2AFB: /* TRIPLE SOLIDUS BINARY RELATION */
        case 0x2AFD: /* DOUBLE SOLIDUS OPERATOR */
        case 0x3008: /* LEFT ANGLE BRACKET */
        case 0x3014: /* LEFT TORTOISE SHELL BRACKET */
        case 0x3015: /* RIGHT TORTOISE SHELL BRACKET */
        case 0x3033: /* VERTICAL KANA REPEAT MARK UPPER HALF */
        case 0x3035: /* VERTICAL KANA REPEAT MARK LOWER HALF */
        case 0x321D: /* PARENTHESIZED KOREAN CHARACTER OJEON */
        case 0x321E: /* PARENTHESIZED KOREAN CHARACTER O HU */
        case 0x33DF: /* SQUARE A OVER M */
        case 0xFE14: /* PRESENTATION FORM FOR VERTICAL SEMICOLON */
        case 0xFE15: /* PRESENTATION FORM FOR VERTICAL EXCLAMATION MARK */
        case 0xFE3F: /* PRESENTATION FORM FOR VERTICAL LEFT ANGLE BRACKET */
        case 0xFE5D: /* SMALL LEFT TORTOISE SHELL BRACKET */
        case 0xFE5E: /* SMALL RIGHT TORTOISE SHELL BRACKET */
            return YES;
        default:
            return NO;
    }
}

static char hexDigit(int i)
{
    if (i < 0 || i > 16) {
        LOG_ERROR("illegal hex digit");
        return '0';
    }
    int h = i;
    if (h >= 10) {
        h = h - 10 + 'A'; 
    }
    else {
        h += '0';
    }
    return h;
}

static BOOL isHexDigit(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

static int hexDigitValue(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    LOG_ERROR("illegal hex digit");
    return 0;
}

static void applyHostNameFunctionToMailToURLString(NSString *string, StringRangeApplierFunction f, void *context)
{
    // In a mailto: URL, host names come after a '@' character and end with a '>' or ',' or '?' character.
    // Skip quoted strings so that characters in them don't confuse us.
    // When we find a '?' character, we are past the part of the URL that contains host names.

    static NSCharacterSet *hostNameOrStringStartCharacters;
    if (hostNameOrStringStartCharacters == nil) {
        hostNameOrStringStartCharacters = [NSCharacterSet characterSetWithCharactersInString:@"\"@?"];
        CFRetain(hostNameOrStringStartCharacters);
    }
    static NSCharacterSet *hostNameEndCharacters;
    if (hostNameEndCharacters == nil) {
        hostNameEndCharacters = [NSCharacterSet characterSetWithCharactersInString:@">,?"];
        CFRetain(hostNameEndCharacters);
    }
    static NSCharacterSet *quotedStringCharacters;
    if (quotedStringCharacters == nil) {
        quotedStringCharacters = [NSCharacterSet characterSetWithCharactersInString:@"\"\\"];
        CFRetain(quotedStringCharacters);
    }

    unsigned stringLength = [string length];
    NSRange remaining = NSMakeRange(0, stringLength);
    
    while (1) {
        // Find start of host name or of quoted string.
        NSRange hostNameOrStringStart = [string rangeOfCharacterFromSet:hostNameOrStringStartCharacters options:0 range:remaining];
        if (hostNameOrStringStart.location == NSNotFound) {
            return;
        }
        unichar c = [string characterAtIndex:hostNameOrStringStart.location];
        remaining.location = NSMaxRange(hostNameOrStringStart);
        remaining.length = stringLength - remaining.location;

        if (c == '?') {
            return;
        }
        
        if (c == '@') {
            // Find end of host name.
            unsigned hostNameStart = remaining.location;
            NSRange hostNameEnd = [string rangeOfCharacterFromSet:hostNameEndCharacters options:0 range:remaining];
            BOOL done;
            if (hostNameEnd.location == NSNotFound) {
                hostNameEnd.location = stringLength;
                done = YES;
            } else {
                remaining.location = hostNameEnd.location;
                remaining.length = stringLength - remaining.location;
                done = NO;
            }

            // Process host name range.
            f(string, NSMakeRange(hostNameStart, hostNameEnd.location - hostNameStart), context);

            if (done) {
                return;
            }
        } else {
            // Skip quoted string.
            ASSERT(c == '"');
            while (1) {
                NSRange escapedCharacterOrStringEnd = [string rangeOfCharacterFromSet:quotedStringCharacters options:0 range:remaining];
                if (escapedCharacterOrStringEnd.location == NSNotFound) {
                    return;
                }
                c = [string characterAtIndex:escapedCharacterOrStringEnd.location];
                remaining.location = NSMaxRange(escapedCharacterOrStringEnd);
                remaining.length = stringLength - remaining.location;
                
                // If we are the end of the string, then break from the string loop back to the host name loop.
                if (c == '"') {
                    break;
                }
                
                // Skip escaped character.
                ASSERT(c == '\\');
                if (remaining.length == 0) {
                    return;
                }                
                remaining.location += 1;
                remaining.length -= 1;
            }
        }
    }
}

static void applyHostNameFunctionToURLString(NSString *string, StringRangeApplierFunction f, void *context)
{
    // Find hostnames. Too bad we can't use any real URL-parsing code to do this,
    // but we have to do it before doing all the %-escaping, and this is the only
    // code we have that parses mailto URLs anyway.

    // Maybe we should implement this using a character buffer instead?

    if ([string _webkit_hasCaseInsensitivePrefix:@"mailto:"]) {
        applyHostNameFunctionToMailToURLString(string, f, context);
        return;
    }

    // Find the host name in a hierarchical URL.
    // It comes after a "://" sequence, with scheme characters preceding.
    // If ends with the end of the string or a ":", "/", or a "?".
    // If there is a "@" character, the host part is just the part after the "@".
    NSRange separatorRange = [string rangeOfString:@"://"];
    if (separatorRange.location == NSNotFound) {
        return;
    }

    // Check that all characters before the :// are valid scheme characters.
    static NSCharacterSet *nonSchemeCharacters;
    if (nonSchemeCharacters == nil) {
        nonSchemeCharacters = [[NSCharacterSet characterSetWithCharactersInString:@"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-."] invertedSet];
        CFRetain(nonSchemeCharacters);
    }
    if ([string rangeOfCharacterFromSet:nonSchemeCharacters options:0 range:NSMakeRange(0, separatorRange.location)].location != NSNotFound) {
        return;
    }

    unsigned stringLength = [string length];

    static NSCharacterSet *hostTerminators;
    if (hostTerminators == nil) {
        hostTerminators = [NSCharacterSet characterSetWithCharactersInString:@":/?#"];
        CFRetain(hostTerminators);
    }

    // Start after the separator.
    unsigned authorityStart = NSMaxRange(separatorRange);

    // Find terminating character.
    NSRange hostNameTerminator = [string rangeOfCharacterFromSet:hostTerminators options:0 range:NSMakeRange(authorityStart, stringLength - authorityStart)];
    unsigned hostNameEnd = hostNameTerminator.location == NSNotFound ? stringLength : hostNameTerminator.location;

    // Find "@" for the start of the host name.
    NSRange userInfoTerminator = [string rangeOfString:@"@" options:0 range:NSMakeRange(authorityStart, hostNameEnd - authorityStart)];
    unsigned hostNameStart = userInfoTerminator.location == NSNotFound ? authorityStart : NSMaxRange(userInfoTerminator);

    f(string, NSMakeRange(hostNameStart, hostNameEnd - hostNameStart), context);
}

@implementation NSURL (WebNSURLExtras)

static void collectRangesThatNeedMapping(NSString *string, NSRange range, void *context, BOOL encode)
{
    BOOL needsMapping = encode
        ? [string _web_hostNameNeedsEncodingWithRange:range]
        : [string _web_hostNameNeedsDecodingWithRange:range];
    if (!needsMapping) {
        return;
    }

    NSMutableArray **array = (NSMutableArray **)context;
    if (*array == nil) {
        *array = [[NSMutableArray alloc] init];
    }

    [*array addObject:[NSValue valueWithRange:range]];
}

static void collectRangesThatNeedEncoding(NSString *string, NSRange range, void *context)
{
    return collectRangesThatNeedMapping(string, range, context, YES);
}

static void collectRangesThatNeedDecoding(NSString *string, NSRange range, void *context)
{
    return collectRangesThatNeedMapping(string, range, context, NO);
}

static NSString *mapHostNames(NSString *string, BOOL encode)
{
    // Generally, we want to optimize for the case where there is one host name that does not need mapping.
    
    if (encode && [string canBeConvertedToEncoding:NSASCIIStringEncoding])
        return string;

    // Make a list of ranges that actually need mapping.
    NSMutableArray *hostNameRanges = nil;
    StringRangeApplierFunction f = encode
        ? collectRangesThatNeedEncoding
        : collectRangesThatNeedDecoding;
    applyHostNameFunctionToURLString(string, f, &hostNameRanges);
    if (hostNameRanges == nil)
        return string;

    // Do the mapping.
    NSMutableString *mutableCopy = [string mutableCopy];
    unsigned i = [hostNameRanges count];
    while (i-- != 0) {
        NSRange hostNameRange = [[hostNameRanges objectAtIndex:i] rangeValue];
        NSString *mappedHostName = encode
            ? [string _web_encodeHostNameWithRange:hostNameRange]
            : [string _web_decodeHostNameWithRange:hostNameRange];
        [mutableCopy replaceCharactersInRange:hostNameRange withString:mappedHostName];
    }
    [hostNameRanges release];
    return [mutableCopy autorelease];
}

+ (NSURL *)_web_URLWithUserTypedString:(NSString *)string relativeToURL:(NSURL *)URL
{
    if (string == nil) {
        return nil;
    }
    string = mapHostNames([string _webkit_stringByTrimmingWhitespace], YES);

    NSData *userTypedData = [string dataUsingEncoding:NSUTF8StringEncoding];
    ASSERT(userTypedData);

    const UInt8 *inBytes = static_cast<const UInt8 *>([userTypedData bytes]);
    int inLength = [userTypedData length];
    if (inLength == 0) {
        return [NSURL URLWithString:@""];
    }
    
    char *outBytes = static_cast<char *>(malloc(inLength * 3)); // large enough to %-escape every character
    char *p = outBytes;
    int outLength = 0;
    int i;
    for (i = 0; i < inLength; i++) {
        UInt8 c = inBytes[i];
        if (c <= 0x20 || c >= 0x7f) {
            *p++ = '%';
            *p++ = hexDigit(c >> 4);
            *p++ = hexDigit(c & 0xf);
            outLength += 3;
        }
        else {
            *p++ = c;
            outLength++;
        }
    }
 
    NSData *data = [NSData dataWithBytesNoCopy:outBytes length:outLength]; // adopts outBytes
    return [self _web_URLWithData:data relativeToURL:URL];
}

+ (NSURL *)_web_URLWithUserTypedString:(NSString *)string
{
    return [self _web_URLWithUserTypedString:string relativeToURL:nil];
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
    return [self _web_URLWithData:data relativeToURL:baseURL];
}

+ (NSURL *)_web_URLWithData:(NSData *)data
{
    return [NSURL _web_URLWithData:data relativeToURL:nil];
}      

+ (NSURL *)_web_URLWithData:(NSData *)data relativeToURL:(NSURL *)baseURL
{
    if (data == nil)
        return nil;
    
    NSURL *result = nil;
    size_t length = [data length];
    if (length > 0) {
        // work around <rdar://4470771>: CFURLCreateAbsoluteURLWithBytes(.., TRUE) doesn't remove non-path components.
        baseURL = [baseURL _webkit_URLByRemovingResourceSpecifier];
        
        const UInt8 *bytes = static_cast<const UInt8*>([data bytes]);

        // CFURLCreateAbsoluteURLWithBytes would complain to console if we passed a path to it.
        if (bytes[0] == '/' && !baseURL)
            return nil;

        // NOTE: We use UTF-8 here since this encoding is used when computing strings when returning URL components
        // (e.g calls to NSURL -path). However, this function is not tolerant of illegal UTF-8 sequences, which
        // could either be a malformed string or bytes in a different encoding, like shift-jis, so we fall back
        // onto using ISO Latin 1 in those cases.
        result = WebCFAutorelease(CFURLCreateAbsoluteURLWithBytes(NULL, bytes, length, kCFStringEncodingUTF8, (CFURLRef)baseURL, YES));
        if (!result)
            result = WebCFAutorelease(CFURLCreateAbsoluteURLWithBytes(NULL, bytes, length, kCFStringEncodingISOLatin1, (CFURLRef)baseURL, YES));
    } else
        result = [NSURL URLWithString:@""];
    
    return result;
}

- (NSData *)_web_originalData
{
    UInt8 *buffer = (UInt8 *)malloc(URL_BYTES_BUFFER_LENGTH);
    CFIndex bytesFilled = CFURLGetBytes((CFURLRef)self, buffer, URL_BYTES_BUFFER_LENGTH);
    if (bytesFilled == -1) {
        CFIndex bytesToAllocate = CFURLGetBytes((CFURLRef)self, NULL, 0);
        buffer = (UInt8 *)realloc(buffer, bytesToAllocate);
        bytesFilled = CFURLGetBytes((CFURLRef)self, buffer, bytesToAllocate);
        ASSERT(bytesFilled == bytesToAllocate);
    }
    
    // buffer is adopted by the NSData
    NSData *data = [NSData dataWithBytesNoCopy:buffer length:bytesFilled freeWhenDone:YES];
    
    NSURL *baseURL = (NSURL *)CFURLGetBaseURL((CFURLRef)self);
    if (baseURL)
        return [[NSURL _web_URLWithData:data relativeToURL:baseURL] _web_originalData];
    return data;
}

- (NSString *)_web_originalDataAsString
{
    return [[[NSString alloc] initWithData:[self _web_originalData] encoding:NSISOLatin1StringEncoding] autorelease];
}

static CFStringRef createStringWithEscapedUnsafeCharacters(CFStringRef string)
{
    CFIndex length = CFStringGetLength(string);
    Vector<UChar, 2048> sourceBuffer(length);
    CFStringGetCharacters(string, CFRangeMake(0, length), sourceBuffer.data());

    Vector<UChar, 2048> outBuffer;

    CFIndex i = 0;
    while (i < length) {
        UChar32 c;
        U16_NEXT(sourceBuffer, i, length, c)

        if (isLookalikeCharacter(c)) {
            uint8_t utf8Buffer[4];
            CFIndex offset = 0;
            UBool failure = false;
            U8_APPEND(utf8Buffer, offset, 4, c, failure)
            ASSERT(!failure);

            for (CFIndex j = 0; j < offset; ++j) {
                outBuffer.append('%');
                outBuffer.append(hexDigit(utf8Buffer[j] >> 4));
                outBuffer.append(hexDigit(utf8Buffer[j] & 0xf));
            }
        } else {
            UChar utf16Buffer[2];
            CFIndex offset = 0;
            UBool failure = false;
            U16_APPEND(utf16Buffer, offset, 2, c, failure)
            ASSERT(!failure);
            for (CFIndex j = 0; j < offset; ++j)
                outBuffer.append(utf16Buffer[j]);
        }
    }

    return CFStringCreateWithCharacters(NULL, outBuffer.data(), outBuffer.size());
}

- (NSString *)_web_userVisibleString
{
    NSData *data = [self _web_originalData];
    const unsigned char *before = static_cast<const unsigned char*>([data bytes]);
    int length = [data length];

    bool needsHostNameDecoding = false;

    const unsigned char *p = before;
    int bufferLength = (length * 3) + 1;
    char *after = static_cast<char *>(malloc(bufferLength)); // large enough to %-escape every character
    char *q = after;
    int i;
    for (i = 0; i < length; i++) {
        unsigned char c = p[i];
        // unescape escape sequences that indicate bytes greater than 0x7f
        if (c == '%' && (i + 1 < length && isHexDigit(p[i + 1])) && i + 2 < length && isHexDigit(p[i + 2])) {
            unsigned char u = (hexDigitValue(p[i + 1]) << 4) | hexDigitValue(p[i + 2]);
            if (u > 0x7f) {
                // unescape
                *q++ = u;
            } else {
                // do not unescape
                *q++ = p[i];
                *q++ = p[i + 1];
                *q++ = p[i + 2];
            }
            i += 2;
        } else {
            *q++ = c;

            // Check for "xn--" in an efficient, non-case-sensitive, way.
            if (c == '-' && i >= 3 && !needsHostNameDecoding && (q[-4] | 0x20) == 'x' && (q[-3] | 0x20) == 'n' && q[-2] == '-')
                needsHostNameDecoding = true;
        }
    }
    *q = '\0';
    
    // Check string to see if it can be converted to display using UTF-8  
    NSString *result = [NSString stringWithUTF8String:after];
    if (!result) {
        // Could not convert to UTF-8.
        // Convert characters greater than 0x7f to escape sequences.
        // Shift current string to the end of the buffer
        // then we will copy back bytes to the start of the buffer 
        // as we convert.
        int afterlength = q - after;
        char *p = after + bufferLength - afterlength - 1;
        memmove(p, after, afterlength + 1); // copies trailing '\0'
        char *q = after;
        while (*p) {
            unsigned char c = *p;
            if (c > 0x7f) {
                *q++ = '%';
                *q++ = hexDigit(c >> 4);
                *q++ = hexDigit(c & 0xf);
            } else {
                *q++ = *p;
            }
            p++;
        }
        *q = '\0';
        result = [NSString stringWithUTF8String:after];
    }

    free(after);

    result = mapHostNames(result, !needsHostNameDecoding);
    result = [result precomposedStringWithCanonicalMapping];
    return WebCFAutorelease(createStringWithEscapedUnsafeCharacters((CFStringRef)result));
}

- (BOOL)_web_isEmpty
{
    if (!CFURLGetBaseURL((CFURLRef)self))
        return CFURLGetBytes((CFURLRef)self, NULL, 0) == 0;
    return [[self _web_originalData] length] == 0;
}

- (const char *)_web_URLCString
{
    NSMutableData *data = [NSMutableData data];
    [data appendData:[self _web_originalData]];
    [data appendBytes:"\0" length:1];
    return (const char *)[data bytes];
 }

- (NSURL *)_webkit_canonicalize
{    
    NSURLRequest *request = [[NSURLRequest alloc] initWithURL:self];
    Class concreteClass = WKNSURLProtocolClassForRequest(request);
    if (!concreteClass) {
        [request release];
        return self;
    }
    
    // This applies NSURL's concept of canonicalization, but not KURL's concept. It would
    // make sense to apply both, but when we tried that it caused a performance degradation
    // (see 5315926). It might make sense to apply only the KURL concept and not the NSURL
    // concept, but it's too risky to make that change for WebKit 3.0.
    NSURLRequest *newRequest = [concreteClass canonicalRequestForRequest:request];
    NSURL *newURL = [newRequest URL]; 
    NSURL *result = [[newURL retain] autorelease]; 
    [request release];
    
    return result;
}

- (NSURL *)_web_URLByTruncatingOneCharacterBeforeComponent:(CFURLComponentType)component
{
    CFRange fragRg = CFURLGetByteRangeForComponent((CFURLRef)self, component, NULL);
    if (fragRg.location == kCFNotFound)
        return self;
 
    UInt8 *urlBytes, buffer[2048];
    CFIndex numBytes = CFURLGetBytes((CFURLRef)self, buffer, 2048);
    if (numBytes == -1) {
        numBytes = CFURLGetBytes((CFURLRef)self, NULL, 0);
        urlBytes = static_cast<UInt8*>(malloc(numBytes));
        CFURLGetBytes((CFURLRef)self, urlBytes, numBytes);
    } else
        urlBytes = buffer;
     
    NSURL *result = (NSURL *)CFMakeCollectable(CFURLCreateWithBytes(NULL, urlBytes, fragRg.location - 1, kCFStringEncodingUTF8, NULL));
    if (!result)
        result = (NSURL *)CFMakeCollectable(CFURLCreateWithBytes(NULL, urlBytes, fragRg.location - 1, kCFStringEncodingISOLatin1, NULL));
     
    if (urlBytes != buffer) free(urlBytes);
    return result ? [result autorelease] : self;
}

- (NSURL *)_webkit_URLByRemovingFragment
{
    return [self _web_URLByTruncatingOneCharacterBeforeComponent:kCFURLComponentFragment];
}

- (NSURL *)_webkit_URLByRemovingResourceSpecifier
{
    return [self _web_URLByTruncatingOneCharacterBeforeComponent:kCFURLComponentResourceSpecifier];
}

- (NSURL *)_web_URLByRemovingComponentAndSubsequentCharacter:(CFURLComponentType)component
{
    CFRange range = CFURLGetByteRangeForComponent((CFURLRef)self, component, 0);
    if (range.location == kCFNotFound)
        return self;

    // Remove one subsequent character.
    ++range.length;

    UInt8* urlBytes;
    UInt8 buffer[2048];
    CFIndex numBytes = CFURLGetBytes((CFURLRef)self, buffer, 2048);
    if (numBytes == -1) {
        numBytes = CFURLGetBytes((CFURLRef)self, NULL, 0);
        urlBytes = static_cast<UInt8*>(malloc(numBytes));
        CFURLGetBytes((CFURLRef)self, urlBytes, numBytes);
    } else
        urlBytes = buffer;

    if (numBytes < range.location)
        return self;
    if (numBytes < range.location + range.length)
        range.length = numBytes - range.location;

    memmove(urlBytes + range.location, urlBytes + range.location + range.length, numBytes - range.location + range.length);

    NSURL *result = (NSURL *)CFMakeCollectable(CFURLCreateWithBytes(NULL, urlBytes, numBytes - range.length, kCFStringEncodingUTF8, NULL));
    if (!result)
        result = (NSURL *)CFMakeCollectable(CFURLCreateWithBytes(NULL, urlBytes, numBytes - range.length, kCFStringEncodingISOLatin1, NULL));

    if (urlBytes != buffer)
        free(urlBytes);

    return result ? [result autorelease] : self;
}

- (NSURL *)_web_URLByRemovingUserInfo
{
    return [self _web_URLByRemovingComponentAndSubsequentCharacter:kCFURLComponentUserInfo];
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
        ? (NSURL *)WebCFAutorelease(CFURLCreateAbsoluteURLWithBytes(NULL, buffer, bytesFilled, kCFStringEncodingUTF8, nil, YES))
        : (NSURL *)self;

    if (buffer != static_buffer) {
        free(buffer);
    }
    
    return result;
}


-(BOOL)_web_hasQuestionMarkOnlyQueryString
{
    CFRange rangeWithSeparators;
    CFURLGetByteRangeForComponent((CFURLRef)self, kCFURLComponentQuery, &rangeWithSeparators);
    if (rangeWithSeparators.location != kCFNotFound && rangeWithSeparators.length == 1) {
        return YES;
    }
    return NO;
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

#define completeURL (CFURLComponentType)-1

-(NSData *)_web_dataForURLComponentType:(CFURLComponentType)componentType
{
    static int URLComponentTypeBufferLength = 2048;
    
    UInt8 staticAllBytesBuffer[URLComponentTypeBufferLength];
    UInt8 *allBytesBuffer = staticAllBytesBuffer;
    
    CFIndex bytesFilled = CFURLGetBytes((CFURLRef)self, allBytesBuffer, URLComponentTypeBufferLength);
    if (bytesFilled == -1) {
        CFIndex bytesToAllocate = CFURLGetBytes((CFURLRef)self, NULL, 0);
        allBytesBuffer = static_cast<UInt8 *>(malloc(bytesToAllocate));
        bytesFilled = CFURLGetBytes((CFURLRef)self, allBytesBuffer, bytesToAllocate);
    }
    
    CFRange range;
    if (componentType != completeURL) {
        range = CFURLGetByteRangeForComponent((CFURLRef)self, componentType, NULL);
        if (range.location == kCFNotFound) {
            return nil;
        }
    }
    else {
        range.location = 0;
        range.length = bytesFilled;
    }
    
    NSData *componentData = [NSData dataWithBytes:allBytesBuffer + range.location length:range.length]; 
    
    const unsigned char *bytes = static_cast<const unsigned char *>([componentData bytes]);
    NSMutableData *resultData = [NSMutableData data];
    // NOTE: add leading '?' to query strings non-zero length query strings.
    // NOTE: retain question-mark only query strings.
    if (componentType == kCFURLComponentQuery) {
        if (range.length > 0 || [self _web_hasQuestionMarkOnlyQueryString]) {
            [resultData appendBytes:"?" length:1];    
        }
    }
    int i;
    for (i = 0; i < range.length; i++) {
        unsigned char c = bytes[i];
        if (c <= 0x20 || c >= 0x7f) {
            char escaped[3];
            escaped[0] = '%';
            escaped[1] = hexDigit(c >> 4);
            escaped[2] = hexDigit(c & 0xf);
            [resultData appendBytes:escaped length:3];    
        }
        else {
            char b[1];
            b[0] = c;
            [resultData appendBytes:b length:1];    
        }               
    }
    
    if (staticAllBytesBuffer != allBytesBuffer) {
        free(allBytesBuffer);
    }
    
    return resultData;
}

-(NSData *)_web_schemeData
{
    return [self _web_dataForURLComponentType:kCFURLComponentScheme];
}

-(NSData *)_web_hostData
{
    NSData *result = [self _web_dataForURLComponentType:kCFURLComponentHost];
    NSData *scheme = [self _web_schemeData];
    // Take off localhost for file
    if ([scheme _web_isCaseInsensitiveEqualToCString:"file"]) {
        return ([result _web_isCaseInsensitiveEqualToCString:"localhost"]) ? nil : result;
    }
    return result;
}

- (NSString *)_web_hostString
{
    NSData *data = [self _web_hostData];
    if (!data) {
        data = [NSData data];
    }
    return [[[NSString alloc] initWithData:[self _web_hostData] encoding:NSUTF8StringEncoding] autorelease];
}

- (NSString *)_webkit_suggestedFilenameWithMIMEType:(NSString *)MIMEType
{
    return suggestedFilenameWithMIMEType(self, MIMEType);
}

@end

@implementation NSString (WebNSURLExtras)

- (BOOL)_web_isUserVisibleURL
{
    BOOL valid = YES;
    // get buffer

    char static_buffer[1024];
    const char *p;
    BOOL success = CFStringGetCString((CFStringRef)self, static_buffer, 1023, kCFStringEncodingUTF8);
    if (success) {
        p = static_buffer;
    } else {
        p = [self UTF8String];
    }

    int length = strlen(p);

    // check for characters <= 0x20 or >=0x7f, %-escape sequences of %7f, and xn--, these
    // are the things that will lead _web_userVisibleString to actually change things.
    int i;
    for (i = 0; i < length; i++) {
        unsigned char c = p[i];
        // escape control characters, space, and delete
        if (c <= 0x20 || c == 0x7f) {
            valid = NO;
            break;
        } else if (c == '%' && (i + 1 < length && isHexDigit(p[i + 1])) && i + 2 < length && isHexDigit(p[i + 2])) {
            unsigned char u = (hexDigitValue(p[i + 1]) << 4) | hexDigitValue(p[i + 2]);
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


static BOOL readIDNScriptWhiteListFile(NSString *filename)
{
    if (!filename) {
        return NO;
    }
    FILE *file = fopen([filename fileSystemRepresentation], "r");
    if (file == NULL) {
        return NO;
    }

    // Read a word at a time.
    // Allow comments, starting with # character to the end of the line.
    while (1) {
        // Skip a comment if present.
        int result = fscanf(file, " #%*[^\n\r]%*[\n\r]");
        if (result == EOF) {
            break;
        }

        // Read a script name if present.
        char word[33];
        result = fscanf(file, " %32[^# \t\n\r]%*[^# \t\n\r] ", word);
        if (result == EOF) {
            break;
        }
        if (result == 1) {
            // Got a word, map to script code and put it into the array.
            int32_t script = u_getPropertyValueEnum(UCHAR_SCRIPT, word);
            if (script >= 0 && script < USCRIPT_CODE_LIMIT) {
                size_t index = script / 32;
                uint32_t mask = 1 << (script % 32);
                IDNScriptWhiteList[index] |= mask;
            }
        }
    }
    fclose(file);
    return YES;
}

static void readIDNScriptWhiteList(void)
{
    // Read white list from library.
    NSArray *dirs = NSSearchPathForDirectoriesInDomains(NSLibraryDirectory, NSAllDomainsMask, YES);
    int i, numDirs = [dirs count];
    for (i = 0; i < numDirs; i++) {
        NSString *dir = [dirs objectAtIndex:i];
        if (readIDNScriptWhiteListFile([dir stringByAppendingPathComponent:@"IDNScriptWhiteList.txt"])) {
            return;
        }
    }

    // Fall back on white list inside bundle.
    NSBundle *bundle = [NSBundle bundleWithIdentifier:@"com.apple.WebKit"];
    readIDNScriptWhiteListFile([bundle pathForResource:@"IDNScriptWhiteList" ofType:@"txt"]);
}

static BOOL allCharactersInIDNScriptWhiteList(const UChar *buffer, int32_t length)
{
    pthread_once(&IDNScriptWhiteListFileRead, readIDNScriptWhiteList);

    int32_t i = 0;
    while (i < length) {
        UChar32 c;
        U16_NEXT(buffer, i, length, c)
        UErrorCode error = U_ZERO_ERROR;
        UScriptCode script = uscript_getScript(c, &error);
        if (error != U_ZERO_ERROR) {
            LOG_ERROR("got ICU error while trying to look at scripts: %d", error);
            return NO;
        }
        if (script < 0) {
            LOG_ERROR("got negative number for script code from ICU: %d", script);
            return NO;
        }
        if (script >= USCRIPT_CODE_LIMIT) {
            return NO;
        }
        size_t index = script / 32;
        uint32_t mask = 1 << (script % 32);
        if (!(IDNScriptWhiteList[index] & mask)) {
            return NO;
        }

        if (isLookalikeCharacter(c))
            return NO;
    }
    return YES;
}

static BOOL allCharactersAllowedByTLDRules(const UChar* buffer, int32_t length)
{
    // Skip trailing dot for root domain.
    if (buffer[length - 1] == '.')
        --length;

    if (length > 3
        && buffer[length - 3] == '.'
        && buffer[length - 2] == 0x0440 // CYRILLIC SMALL LETTER ER
        && buffer[length - 1] == 0x0444) // CYRILLIC SMALL LETTER EF
    {
        // Rules defined by <http://www.cctld.ru/ru/docs/rulesrf.php>. This code only checks requirements that matter for presentation purposes.
        for (int32_t i = length - 4; i; --i) {
            UChar ch = buffer[i];

            // Only modern Russian letters, digits and dashes are allowed.
            if ((ch >= 0x0430 && ch <= 0x044f)
                || ch == 0x0451
                || (ch >= '0' && ch <= '9')
                || ch == '-')
                continue;

            // Only check top level domain. Lower level registrars may have different rules.
            if (ch == '.')
                break;

            return NO;
        }
        return YES;
    }

    // Not a known top level domain with special rules.
    return NO;
}

// Return value of nil means no mapping is necessary.
// If makeString is NO, then return value is either nil or self to indicate mapping is necessary.
// If makeString is YES, then return value is either nil or the mapped string.
- (NSString *)_web_mapHostNameWithRange:(NSRange)range encode:(BOOL)encode makeString:(BOOL)makeString
{
    if (range.length > HOST_NAME_BUFFER_LENGTH) {
        return nil;
    }

    if ([self length] == 0)
        return nil;
    
    UChar sourceBuffer[HOST_NAME_BUFFER_LENGTH];
    UChar destinationBuffer[HOST_NAME_BUFFER_LENGTH];
    
    NSString *string = self;
    if (encode && [self rangeOfString:@"%" options:NSLiteralSearch range:range].location != NSNotFound) {
        NSString *substring = [self substringWithRange:range];
        substring = WebCFAutorelease(CFURLCreateStringByReplacingPercentEscapes(NULL, (CFStringRef)substring, CFSTR("")));
        if (substring != nil) {
            string = substring;
            range = NSMakeRange(0, [string length]);
        }
    }
    
    int length = range.length;
    [string getCharacters:sourceBuffer range:range];

    UErrorCode error = U_ZERO_ERROR;
    int32_t numCharactersConverted = (encode ? uidna_IDNToASCII : uidna_IDNToUnicode)
        (sourceBuffer, length, destinationBuffer, HOST_NAME_BUFFER_LENGTH, UIDNA_ALLOW_UNASSIGNED, NULL, &error);
    if (error != U_ZERO_ERROR) {
        return nil;
    }
    if (numCharactersConverted == length && memcmp(sourceBuffer, destinationBuffer, length * sizeof(UChar)) == 0) {
        return nil;
    }
    if (!encode && !allCharactersInIDNScriptWhiteList(destinationBuffer, numCharactersConverted) && !allCharactersAllowedByTLDRules(destinationBuffer, numCharactersConverted)) {
        return nil;
    }
    return makeString ? (NSString *)[NSString stringWithCharacters:destinationBuffer length:numCharactersConverted] : (NSString *)self;
}

- (BOOL)_web_hostNameNeedsDecodingWithRange:(NSRange)range
{
    return [self _web_mapHostNameWithRange:range encode:NO makeString:NO] != nil;
}

- (BOOL)_web_hostNameNeedsEncodingWithRange:(NSRange)range
{
    return [self _web_mapHostNameWithRange:range encode:YES makeString:NO] != nil;
}

- (NSString *)_web_decodeHostNameWithRange:(NSRange)range
{
    return [self _web_mapHostNameWithRange:range encode:NO makeString:YES];
}

- (NSString *)_web_encodeHostNameWithRange:(NSRange)range
{
    return [self _web_mapHostNameWithRange:range encode:YES makeString:YES];
}

- (NSString *)_web_decodeHostName
{
    NSString *name = [self _web_mapHostNameWithRange:NSMakeRange(0, [self length]) encode:NO makeString:YES];
    return name == nil ? self : name;
}

- (NSString *)_web_encodeHostName
{
    NSString *name = [self _web_mapHostNameWithRange:NSMakeRange(0, [self length]) encode:YES makeString:YES];
    return name == nil ? self : name;
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

@end
