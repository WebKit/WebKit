/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#import "config.h"
#import "PlatformPasteboard.h"

#import "Pasteboard.h"
#import "PasteboardItemInfo.h"
#import "WebCoreNSURLExtras.h"

#if PLATFORM(IOS_FAMILY)
#import "AbstractPasteboard.h"
#else
#import "LegacyNSPasteboardTypes.h"
#endif

namespace WebCore {

Optional<Vector<PasteboardItemInfo>> PlatformPasteboard::allPasteboardItemInfo(int64_t changeCount)
{
    if (changeCount != [m_pasteboard changeCount])
        return WTF::nullopt;

    Vector<PasteboardItemInfo> itemInfo;
    int numberOfItems = count();
    itemInfo.reserveInitialCapacity(numberOfItems);
    for (NSInteger itemIndex = 0; itemIndex < numberOfItems; ++itemIndex) {
        auto item = informationForItemAtIndex(itemIndex, changeCount);
        if (!item)
            return WTF::nullopt;

        itemInfo.uncheckedAppend(WTFMove(*item));
    }
    return itemInfo;
}

bool PlatformPasteboard::containsStringSafeForDOMToReadForType(const String& type) const
{
    return !stringForType(type).isEmpty();
}

String PlatformPasteboard::urlStringSuitableForLoading(String& title)
{
#if PLATFORM(MAC)
    String URLTitleString = stringForType(String(WebURLNamePboardType));
    if (!URLTitleString.isEmpty())
        title = URLTitleString;
#endif

    Vector<String> types;
    getTypes(types);

#if PLATFORM(IOS_FAMILY)
    UNUSED_PARAM(title);
    String urlPasteboardType = kUTTypeURL;
    String stringPasteboardType = kUTTypeText;
#else
    String urlPasteboardType = legacyURLPasteboardType();
    String stringPasteboardType = legacyStringPasteboardType();
#endif

    if (types.contains(urlPasteboardType)) {
        NSURL *URLFromPasteboard = [NSURL URLWithString:stringForType(urlPasteboardType)];
        // Cannot drop other schemes unless <rdar://problem/10562662> and <rdar://problem/11187315> are fixed.
        if (URL { URLFromPasteboard }.protocolIsInHTTPFamily())
            return [URLByCanonicalizingURL(URLFromPasteboard) absoluteString];
    }

    if (types.contains(stringPasteboardType)) {
        NSURL *URLFromPasteboard = [NSURL URLWithString:stringForType(stringPasteboardType)];
        // Pasteboard content is not trusted, because JavaScript code can modify it. We can sanitize it for URLs and other typed content, but not for strings.
        // The result of this function is used to initiate navigation, so we shouldn't allow arbitrary file URLs.
        // FIXME: Should we allow only http family schemes, or anything non-local?
        if (URL { URLFromPasteboard }.protocolIsInHTTPFamily())
            return [URLByCanonicalizingURL(URLFromPasteboard) absoluteString];
    }

#if PLATFORM(MAC)
    if (types.contains(String(legacyFilenamesPasteboardType()))) {
        Vector<String> files;
        getPathnamesForType(files, String(legacyFilenamesPasteboardType()));
        if (files.size() == 1) {
            BOOL isDirectory;
            if ([[NSFileManager defaultManager] fileExistsAtPath:files[0] isDirectory:&isDirectory] && isDirectory)
                return String();
            return [URLByCanonicalizingURL([NSURL fileURLWithPath:files[0]]) absoluteString];
        }
    }
#endif

    return { };
}

} // namespace WebCore
