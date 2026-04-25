/*
 * Copyright (C) 2005-2018 Apple Inc. All rights reserved.
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
#import <wtf/RetainPtr.h>
#import <wtf/cocoa/SpanCocoa.h>
#import <wtf/text/ParsingUtilities.h>
#import <wtf/text/StringCommon.h>

@implementation NSData (WebNSDataExtras)

- (NSString *)_webkit_guessedMIMETypeForXML
{
    auto bytes = span(self);

    constexpr size_t channelTagLength = 7;

    size_t remaining = std::min<size_t>(bytes.size(), WEB_GUESS_MIME_TYPE_PEEK_LENGTH) - (channelTagLength - 1);
    bytes = bytes.first(remaining);

    BOOL foundRDF = false;

    while (!bytes.empty()) {
        // Look for a "<".
        auto hitIndex = WTF::find(bytes, '<');
        if (hitIndex == notFound)
            break;

        // We are trying to identify RSS or Atom. RSS has a top-level
        // element of either <rss> or <rdf>. However, there are
        // non-RSS RDF files, so in the case of <rdf> we further look
        // for a <channel> element. In the case of an Atom file, a
        // top-level <feed> element is all we need to see. Only tags
        // starting with <? or <! can precede the root element. We
        // bail if we don't find an <rss>, <feed> or <rdf> element
        // right after those.

        auto hit = bytes.subspan(hitIndex);
        if (foundRDF) {
            if (spanHasPrefixIgnoringASCIICase(hit, "<channel"_span))
                return @"application/rss+xml";
        } else if (spanHasPrefixIgnoringASCIICase(hit, "<rdf"_span))
            foundRDF = TRUE;
        else if (spanHasPrefixIgnoringASCIICase(hit, "<rss"_span))
            return @"application/rss+xml";
        else if (spanHasPrefixIgnoringASCIICase(hit, "<feed"_span))
            return @"application/atom+xml";
        else if (!spanHasPrefixIgnoringASCIICase(hit, "<?"_span) && !spanHasPrefixIgnoringASCIICase(hit, "<!"_span))
            return nil;

        // Skip the "<" and continue.
        skip(bytes, hitIndex + 1);
    }

    return nil;
}

- (NSString *)_webkit_guessedMIMEType
{
    constexpr size_t scriptTagLength = 7;
    constexpr size_t textHTMLLength = 9;

    NSString *MIMEType = [self _webkit_guessedMIMETypeForXML];
    if ([MIMEType length])
        return MIMEType;

    auto bytes = span(self);

    size_t remaining = std::min<size_t>(bytes.size(), WEB_GUESS_MIME_TYPE_PEEK_LENGTH) - (scriptTagLength - 1);
    auto cursor = bytes.first(remaining);
    while (!cursor.empty()) {
        // Look for a "<".
        size_t hitIndex = WTF::find(cursor, '<');
        if (hitIndex == notFound)
            break;

        auto hit = cursor.subspan(hitIndex);
        // If we found a "<", look for "<html>" or "<a " or "<script".
        if (spanHasPrefixIgnoringASCIICase(hit, "<html>"_span)
            || spanHasPrefixIgnoringASCIICase(hit, "<a "_span)
            || spanHasPrefixIgnoringASCIICase(hit, "<script"_span)
            || spanHasPrefixIgnoringASCIICase(hit, "<title>"_span)) {
            return @"text/html";
        }

        // Skip the "<" and continue.
        skip(cursor, hitIndex + 1);
    }

    // Test for a broken server which has sent the content type as part of the content.
    // This code could be improved to look for other mime types.
    remaining = std::min<size_t>(bytes.size(), WEB_GUESS_MIME_TYPE_PEEK_LENGTH) - (textHTMLLength - 1);
    cursor = bytes.first(remaining);
    while (!cursor.empty()) {
        // Look for a "t" or "T".
        size_t lowerHitIndex = WTF::find(cursor, 't');
        size_t upperHitIndex = WTF::find(cursor, 'T');
        if (lowerHitIndex == notFound && upperHitIndex == notFound)
            break;

        static_assert(notFound == std::numeric_limits<size_t>::max());
        size_t hitIndex = std::min(lowerHitIndex, upperHitIndex);
        auto hit = cursor.subspan(hitIndex);

        // If we found a "t/T", look for "text/html".
        if (spanHasPrefixIgnoringASCIICase(hit, "text/html"_span))
            return @"text/html";

        // Skip the "t/T" and continue.
        skip(cursor, hitIndex + 1);
    }

    if (spanHasPrefix(bytes, "BEGIN:VCARD"_span))
        return @"text/vcard";
    if (spanHasPrefix(bytes, "BEGIN:VCALENDAR"_span))
        return @"text/calendar";

    // Test for plain text.
    bool foundBadCharacter = false;
    for (auto c : bytes) {
        if ((c < 0x20 || c > 0x7E) && (c != '\t' && c != '\r' && c != '\n')) {
            foundBadCharacter = true;
            break;
        }
    }
    if (!foundBadCharacter) {
        // Didn't encounter any bad characters, looks like plain text.
        return @"text/plain";
    }

    // Looks like this is a binary file.

    // Sniff for the JPEG magic number.
    constexpr std::array<uint8_t, 4> jpegMagicNumber { 0xFF, 0xD8, 0xFF, 0xE0 };
    if (spanHasPrefix(bytes, std::span { jpegMagicNumber }))
        return @"image/jpeg";

    return nil;
}

- (BOOL)_web_isCaseInsensitiveEqualToCString:(const char *)string
{
    ASSERT(string);
    return equalLettersIgnoringASCIICase(span(self), unsafeSpan(string));
}

@end
