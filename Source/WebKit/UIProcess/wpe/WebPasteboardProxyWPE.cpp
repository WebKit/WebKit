/*
 * Copyright (C) 2015 Igalia S.L.
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
#include "WebPasteboardProxy.h"

#include <WebCore/PlatformPasteboard.h>
#include <wtf/CompletionHandler.h>
#include <wtf/text/WTFString.h>

namespace WebKit {
using namespace WebCore;

void WebPasteboardProxy::getPasteboardTypes(CompletionHandler<void(Vector<String>&&)>&& completionHandler)
{
    Vector<String> pasteboardTypes;
    PlatformPasteboard().getTypes(pasteboardTypes);
    completionHandler(WTFMove(pasteboardTypes));
}

void WebPasteboardProxy::readStringFromPasteboard(uint64_t index, const String& pasteboardType, CompletionHandler<void(String&&)>&& completionHandler)
{
    completionHandler(PlatformPasteboard().readString(index, pasteboardType));
}

void WebPasteboardProxy::writeWebContentToPasteboard(const WebCore::PasteboardWebContent& content)
{
    PlatformPasteboard().write(content);
}

void WebPasteboardProxy::writeStringToPasteboard(const String& pasteboardType, const String& text)
{
    PlatformPasteboard().write(pasteboardType, text);
}

} // namespace WebKit
