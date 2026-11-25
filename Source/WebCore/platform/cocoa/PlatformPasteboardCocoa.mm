/*
 * Copyright (C) 2019-2024 Apple Inc. All rights reserved.
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
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#if PLATFORM(IOS_FAMILY)
#import "AbstractPasteboard.h"
#else
#import "LegacyNSPasteboardTypes.h"
#endif

namespace WebCore {

std::optional<Vector<PasteboardItemInfo>> PlatformPasteboard::allPasteboardItemInfo(int64_t changeCount)
{
    if (changeCount != [m_pasteboard changeCount])
        return std::nullopt;

    int numberOfItems = count();
    Vector<PasteboardItemInfo> itemInfo(ReserveCapacityTag { }, numberOfItems);
    for (NSInteger itemIndex = 0; itemIndex < numberOfItems; ++itemIndex) {
        auto item = informationForItemAtIndex(itemIndex, changeCount);
        if (!item)
            return std::nullopt;

        itemInfo.append(WTF::move(*item));
    }
    return itemInfo;
}

bool PlatformPasteboard::containsStringSafeForDOMToReadForType(StringImpl* type) const
{
    return !isEmpty(stringForType(type));
}

RefPtr<StringImpl> PlatformPasteboard::urlStringSuitableForLoading(RefPtr<StringImpl>& title)
{
#if PLATFORM(MAC)
    RefPtr<StringImpl> URLTitleString = stringForType(StringImpl::create(WebURLNamePboardType).ptr());
    if (!isEmpty(URLTitleString))
        title = URLTitleString;
#endif

    Vector<RefPtr<StringImpl>> types;
    getTypes(types);

#if PLATFORM(IOS_FAMILY)
    UNUSED_PARAM(title);
    RefPtr<StringImpl> urlPasteboardType = StringImpl::create(UTTypeURL.identifier);
    RefPtr<StringImpl> stringPasteboardType = StringImpl::create(UTTypeText.identifier);
#else
    RefPtr<StringImpl> urlPasteboardType = StringImpl::create(legacyURLPasteboardTypeSingleton());
    RefPtr<StringImpl> stringPasteboardType = StringImpl::create(legacyStringPasteboardTypeSingleton());
#endif

    if (types.containsIf([&](auto& item) { return equal(item, urlPasteboardType); })) {
        RetainPtr urlFromPasteboard = [NSURL URLWithString:createNSString(stringForType(urlPasteboardType)).get()];
        // Cannot drop other schemes unless <rdar://problem/10562662> and <rdar://problem/11187315> are fixed.
        if (URL { urlFromPasteboard }.protocolIsInHTTPFamily())
            return StringImpl::create([protect(URLByCanonicalizingURL(urlFromPasteboard)) absoluteString]);
    }

    if (types.containsIf([&](auto& item) { return equal(item, stringPasteboardType); })) {
        RetainPtr urlFromPasteboard = [NSURL URLWithString:createNSString(stringForType(stringPasteboardType)).get()];
        // Pasteboard content is not trusted, because JavaScript code can modify it. We can sanitize it for URLs and other typed content, but not for strings.
        // The result of this function is used to initiate navigation, so we shouldn't allow arbitrary file URLs.
        // FIXME: Should we allow only http family schemes, or anything non-local?
        if (URL { urlFromPasteboard }.protocolIsInHTTPFamily())
            return StringImpl::create([protect(URLByCanonicalizingURL(urlFromPasteboard)) absoluteString]);
    }

#if PLATFORM(MAC)
    if (types.containsIf([&](auto& item) { return equal(item, StringImpl::create(legacyFilenamesPasteboardTypeSingleton()).get()); })) {
        Vector<RefPtr<StringImpl>> files;
        getPathnamesForType(files, StringImpl::create(legacyFilenamesPasteboardTypeSingleton()).get());
        if (files.size() == 1) {
            RetainPtr firstFile = createNSString(files[0]);
            BOOL isDirectory;
            if ([[NSFileManager defaultManager] fileExistsAtPath:firstFile.get() isDirectory:&isDirectory] && isDirectory)
                return nullptr;
            return StringImpl::create([protect(URLByCanonicalizingURL([NSURL fileURLWithPath:firstFile.get()])) absoluteString]);
        }
    }
#endif

    return { };
}

} // namespace WebCore
