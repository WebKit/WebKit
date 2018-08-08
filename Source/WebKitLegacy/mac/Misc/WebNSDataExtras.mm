/*
 * Copyright (C) 2005-2018 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import <WebKitLegacy/WebNSDataExtras.h>
#import <WebKitLegacy/WebNSDataExtrasPrivate.h>

#import <wtf/Assertions.h>

@interface NSString (WebNSDataExtrasInternal)
- (NSString *)_web_capitalizeRFC822HeaderFieldName;
@end

@implementation NSString (WebNSDataExtrasInternal)

- (NSString *)_web_capitalizeRFC822HeaderFieldName
{
    CFStringRef name = (__bridge CFStringRef)self;
    NSString *result = nil;

    CFIndex len = CFStringGetLength(name);
    char* charPtr = nullptr;
    UniChar* uniCharPtr = nullptr;
    Boolean useUniCharPtr = FALSE;
    Boolean shouldCapitalize = TRUE;
    Boolean somethingChanged = FALSE;

    for (CFIndex i = 0; i < len; i ++) {
        UniChar ch = CFStringGetCharacterAtIndex(name, i);
        Boolean replace = FALSE;
        if (shouldCapitalize && ch >= 'a' && ch <= 'z') {
            ch = ch + 'A' - 'a';
            replace = TRUE;
        } else if (!shouldCapitalize && ch >= 'A' && ch <= 'Z') {
            ch = ch + 'a' - 'A';
            replace = TRUE;
        }
        if (replace) {
            if (!somethingChanged) {
                somethingChanged = TRUE;
                if (CFStringGetBytes(name, CFRangeMake(0, len), kCFStringEncodingISOLatin1, 0, FALSE, NULL, 0, NULL) == len) {
                    // Can be encoded in ISOLatin1
                    useUniCharPtr = FALSE;
                    charPtr = static_cast<char*>(CFAllocatorAllocate(kCFAllocatorDefault, len + 1, 0));
                    CFStringGetCString(name, charPtr, len+1, kCFStringEncodingISOLatin1);
                } else {
                    useUniCharPtr = TRUE;
                    uniCharPtr = static_cast<UniChar*>(CFAllocatorAllocate(kCFAllocatorDefault, len * sizeof(UniChar), 0));
                    CFStringGetCharacters(name, CFRangeMake(0, len), uniCharPtr);
                }
            }
            if (useUniCharPtr)
                uniCharPtr[i] = ch;
            else
                charPtr[i] = ch;
        }
        if (ch == '-')
            shouldCapitalize = TRUE;
        else
            shouldCapitalize = FALSE;
    }
    if (somethingChanged) {
        if (useUniCharPtr)
            result = CFBridgingRelease(CFStringCreateWithCharactersNoCopy(kCFAllocatorDefault, uniCharPtr, len, nullptr));
        else
            result = CFBridgingRelease(CFStringCreateWithCStringNoCopy(kCFAllocatorDefault, charPtr, kCFStringEncodingISOLatin1, nullptr));
    } else
        result = self;

    return result;
}

@end

@implementation NSData (WebKitExtras)

- (NSString *)_webkit_guessedMIMETypeForXML
{
    NSUInteger length = [self length];
    const UInt8* bytes = static_cast<const UInt8*>([self bytes]);

#define CHANNEL_TAG_LENGTH 7

    const char* p = reinterpret_cast<const char*>(bytes);
    int remaining = std::min<NSUInteger>(length, WEB_GUESS_MIME_TYPE_PEEK_LENGTH) - (CHANNEL_TAG_LENGTH - 1);

    BOOL foundRDF = false;

    while (remaining > 0) {
        // Look for a "<".
        const char* hit = static_cast<const char*>(memchr(p, '<', remaining));
        if (!hit)
            break;

        // We are trying to identify RSS or Atom. RSS has a top-level
        // element of either <rss> or <rdf>. However, there are
        // non-RSS RDF files, so in the case of <rdf> we further look
        // for a <channel> element. In the case of an Atom file, a
        // top-level <feed> element is all we need to see. Only tags
        // starting with <? or <! can precede the root element. We
        // bail if we don't find an <rss>, <feed> or <rdf> element
        // right after those.

        if (foundRDF) {
            if (!strncasecmp(hit, "<channel", strlen("<channel")))
                return @"application/rss+xml";
        } else if (!strncasecmp(hit, "<rdf", strlen("<rdf")))
            foundRDF = TRUE;
        else if (!strncasecmp(hit, "<rss", strlen("<rss")))
            return @"application/rss+xml";
        else if (!strncasecmp(hit, "<feed", strlen("<feed")))
            return @"application/atom+xml";
        else if (strncasecmp(hit, "<?", strlen("<?")) && strncasecmp(hit, "<!", strlen("<!")))
            return nil;

        // Skip the "<" and continue.
        remaining -= (hit + 1) - p;
        p = hit + 1;
    }

    return nil;
}

- (NSString *)_webkit_guessedMIMEType
{
#define JPEG_MAGIC_NUMBER_LENGTH 4
#define SCRIPT_TAG_LENGTH 7
#define TEXT_HTML_LENGTH 9
#define VCARD_HEADER_LENGTH 11
#define VCAL_HEADER_LENGTH 15

    NSString *MIMEType = [self _webkit_guessedMIMETypeForXML];
    if ([MIMEType length])
        return MIMEType;

    NSUInteger length = [self length];
    const char* bytes = static_cast<const char*>([self bytes]);

    const char* p = bytes;
    int remaining = std::min<NSUInteger>(length, WEB_GUESS_MIME_TYPE_PEEK_LENGTH) - (SCRIPT_TAG_LENGTH - 1);
    while (remaining > 0) {
        // Look for a "<".
        const char* hit = static_cast<const char*>(memchr(p, '<', remaining));
        if (!hit)
            break;

        // If we found a "<", look for "<html>" or "<a " or "<script".
        if (!strncasecmp(hit, "<html>", strlen("<html>"))
            || !strncasecmp(hit, "<a ", strlen("<a "))
            || !strncasecmp(hit, "<script", strlen("<script"))
            || !strncasecmp(hit, "<title>", strlen("<title>"))) {
            return @"text/html";
        }

        // Skip the "<" and continue.
        remaining -= (hit + 1) - p;
        p = hit + 1;
    }

    // Test for a broken server which has sent the content type as part of the content.
    // This code could be improved to look for other mime types.
    p = bytes;
    remaining = std::min<NSUInteger>(length, WEB_GUESS_MIME_TYPE_PEEK_LENGTH) - (TEXT_HTML_LENGTH - 1);
    while (remaining > 0) {
        // Look for a "t" or "T".
        const char* hit = nullptr;
        const char* lowerhit = static_cast<const char*>(memchr(p, 't', remaining));
        const char* upperhit = static_cast<const char*>(memchr(p, 'T', remaining));
        if (!lowerhit && !upperhit)
            break;

        if (!lowerhit)
            hit = upperhit;
        else if (!upperhit)
            hit = lowerhit;
        else
            hit = std::min<const char*>(lowerhit, upperhit);

        // If we found a "t/T", look for "text/html".
        if (!strncasecmp(hit, "text/html", TEXT_HTML_LENGTH))
            return @"text/html";

        // Skip the "t/T" and continue.
        remaining -= (hit + 1) - p;
        p = hit + 1;
    }

    if ((length >= VCARD_HEADER_LENGTH) && !strncmp(bytes, "BEGIN:VCARD", VCARD_HEADER_LENGTH))
        return @"text/vcard";
    if ((length >= VCAL_HEADER_LENGTH) && !strncmp(bytes, "BEGIN:VCALENDAR", VCAL_HEADER_LENGTH))
        return @"text/calendar";

    // Test for plain text.
    NSUInteger i;
    for (i = 0; i < length; ++i) {
        char c = bytes[i];
        if ((c < 0x20 || c > 0x7E) && (c != '\t' && c != '\r' && c != '\n'))
            break;
    }
    if (i == length) {
        // Didn't encounter any bad characters, looks like plain text.
        return @"text/plain";
    }

    // Looks like this is a binary file.

    // Sniff for the JPEG magic number.
    if ((length >= JPEG_MAGIC_NUMBER_LENGTH) && !strncmp(bytes, "\xFF\xD8\xFF\xE0", JPEG_MAGIC_NUMBER_LENGTH))
        return @"image/jpeg";

#undef JPEG_MAGIC_NUMBER_LENGTH
#undef SCRIPT_TAG_LENGTH
#undef TEXT_HTML_LENGTH
#undef VCARD_HEADER_LENGTH
#undef VCAL_HEADER_LENGTH

    return nil;
}

@end

@implementation NSData (WebNSDataExtras)

- (BOOL)_web_isCaseInsensitiveEqualToCString:(const char *)string
{
    ASSERT(string);

    const char* bytes = static_cast<const char*>([self bytes]);
    return !strncasecmp(bytes, string, [self length]);
}

static const UInt8 *_findEOL(const UInt8 *bytes, CFIndex len)
{
    // According to the HTTP specification EOL is defined as
    // a CRLF pair. Unfortunately, some servers will use LF
    // instead. Worse yet, some servers will use a combination
    // of both (e.g. <header>CRLFLF<body>), so findEOL needs
    // to be more forgiving. It will now accept CRLF, LF, or
    // CR.
    //
    // It returns NULL if EOL is not found or it will return
    // a pointer to the first terminating character.
    for (CFIndex i = 0;  i < len; i++) {
        UInt8 c = bytes[i];
        if ('\n' == c)
            return bytes + i;
        if ('\r' == c) {
            // Check to see if spanning buffer bounds
            // (CRLF is across reads). If so, wait for
            // next read.
            if (i + 1 == len)
                break;

            return bytes + i;
        }
    }

    return nullptr;
}

- (NSMutableDictionary *)_webkit_parseRFC822HeaderFields
{
    NSMutableDictionary *headerFields = [NSMutableDictionary dictionary];

    const UInt8* bytes = static_cast<const UInt8*>([self bytes]);
    NSUInteger length = [self length];
    NSString *lastKey = nil;
    const UInt8 *eol;

    // Loop over lines until we're past the header, or we can't find any more end-of-lines
    while ((eol = _findEOL(bytes, length))) {
        const UInt8 *line = bytes;
        SInt32 lineLength = eol - bytes;

        // Move bytes to the character after the terminator as returned by _findEOL.
        bytes = eol + 1;
        if (('\r' == *eol) && ('\n' == *bytes))
            bytes++; // Safe since _findEOL won't return a spanning CRLF.

        length -= (bytes - line);
        if (!lineLength) {
            // Blank line; we're at the end of the header
            break;
        }
        if (*line == ' ' || *line == '\t') {
            // Continuation of the previous header
            if (!lastKey) {
                // malformed header; ignore it and continue
                continue;
            }
            // Merge the continuation of the previous header
            NSString *currentValue = [headerFields objectForKey:lastKey];
            NSString *newValue = [[NSString alloc] initWithBytes:line length:lineLength encoding:NSISOLatin1StringEncoding];
            ASSERT(currentValue);
            ASSERT(newValue);
            [headerFields setObject:[currentValue stringByAppendingString:newValue] forKey:lastKey];
            [newValue release];
        } else {
            // Brand new header
            const UInt8* colon;
            for (colon = line; *colon != ':' && colon != eol; colon++) { }
            if (colon == eol) {
                // malformed header; ignore it and continue
                continue;
            }
            lastKey = [[NSString alloc] initWithBytes:line length:colon - line encoding:NSISOLatin1StringEncoding];
            [lastKey autorelease];
            lastKey = [lastKey _web_capitalizeRFC822HeaderFieldName];
            for (colon++; colon != eol; colon++) {
                if (*colon != ' ' && *colon != '\t')
                    break;
            }
            NSString *value = [[NSString alloc] initWithBytes:colon length:eol - colon encoding:NSISOLatin1StringEncoding];
            if (NSString *oldValue = [headerFields objectForKey:lastKey]) {
                [value autorelease];
                value = [[NSString alloc] initWithFormat:@"%@, %@", oldValue, value];
            }
            [headerFields setObject:value forKey:lastKey];
            [value release];
        }
    }

    return headerFields;
}

- (BOOL)_web_startsWithBlankLine
{
    return [self length] > 0 && ((const char *)[self bytes])[0] == '\n';
}

- (NSInteger)_web_locationAfterFirstBlankLine
{
    const char *bytes = (const char *)[self bytes];
    NSUInteger length = [self length];

    unsigned i;
    for (i = 0; i < length - 4; i++) {

        //  Support for Acrobat. It sends "\n\n".
        if (bytes[i] == '\n' && bytes[i+1] == '\n')
            return i+2;

        // Returns the position after 2 CRLF's or 1 CRLF if it is the first line.
        if (bytes[i] == '\r' && bytes[i+1] == '\n') {
            i += 2;
            if (i == 2)
                return i;
            if (bytes[i] == '\n') {
                // Support for Director. It sends "\r\n\n" (3880387).
                return i+1;
            }
            if (bytes[i] == '\r' && bytes[i+1] == '\n') {
                // Support for Flash. It sends "\r\n\r\n" (3758113).
                return i+2;
            }
        }
    }
    return NSNotFound;
}

@end
