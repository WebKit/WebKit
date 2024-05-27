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

#import <wtf/Assertions.h>

@implementation NSData (WebNSDataExtras)

- (NSString *)_webkit_guessedMIMETypeForXML
{
    NSUInteger length = [self length];
    const UInt8* bytes = static_cast<const UInt8*>([self bytes]);

#define CHANNEL_TAG_LENGTH 7

    const char* p = byteCast<char>(bytes);
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

- (BOOL)_web_isCaseInsensitiveEqualToCString:(const char *)string
{
    ASSERT(string);

    const char* bytes = static_cast<const char*>([self bytes]);
    return !strncasecmp(bytes, string, [self length]);
}

@end
