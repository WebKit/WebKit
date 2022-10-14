/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "config.h"
#include "Pasteboard.h"

#include "CommonAtomStrings.h"
#include "PasteboardStrategy.h"
#include "PlatformStrategies.h"
#include "Settings.h"
#include "SharedBuffer.h"
#include <wtf/text/StringHash.h>

namespace WebCore {

bool Pasteboard::isSafeTypeForDOMToReadAndWrite(const String& type)
{
    return type == textPlainContentTypeAtom() || type == "text/html"_s || type == "text/uri-list"_s;
}

bool Pasteboard::canExposeURLToDOMWhenPasteboardContainsFiles(const String& urlString)
{
    URL url({ }, urlString);
    return url.protocolIsInHTTPFamily() || url.protocolIsBlob() || url.protocolIsData();
}

#if !PLATFORM(COCOA)

Vector<String> Pasteboard::readAllStrings(const String& type)
{
    auto result = readString(type);
    if (result.isEmpty())
        return { };

    return { result };
}

#endif

std::optional<Vector<PasteboardItemInfo>> Pasteboard::allPasteboardItemInfo() const
{
#if PLATFORM(COCOA)
    if (auto* strategy = platformStrategies()->pasteboardStrategy())
        return strategy->allPasteboardItemInfo(name(), m_changeCount, context());
#endif
    return std::nullopt;
}

std::optional<PasteboardItemInfo> Pasteboard::pasteboardItemInfo(size_t index) const
{
#if PLATFORM(COCOA)
    if (auto* strategy = platformStrategies()->pasteboardStrategy())
        return strategy->informationForItemAtIndex(index, name(), m_changeCount, context());
#else
    UNUSED_PARAM(index);
#endif
    return std::nullopt;
}

String Pasteboard::readString(size_t index, const String& type)
{
    if (auto* strategy = platformStrategies()->pasteboardStrategy())
        return strategy->readStringFromPasteboard(index, type, name(), context());
    return { };
}

RefPtr<WebCore::SharedBuffer> Pasteboard::readBuffer(std::optional<size_t> index, const String& type)
{
    if (auto* strategy = platformStrategies()->pasteboardStrategy())
        return strategy->readBufferFromPasteboard(index, type, name(), context());
    return nullptr;
}

URL Pasteboard::readURL(size_t index, String& title)
{
    if (auto* strategy = platformStrategies()->pasteboardStrategy())
        return strategy->readURLFromPasteboard(index, name(), title, context());
    return { };
}

#if !PLATFORM(MAC)

bool Pasteboard::canWriteTrustworthyWebURLsPboardType()
{
    return false;
}

#endif

};
