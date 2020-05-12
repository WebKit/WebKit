/*
 * Copyright (C) 2016 Red Hat Inc.
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

#include "Clipboard.h"
#include "SharedBufferDataReference.h"
#include "WebFrameProxy.h"
#include "WebSelectionData.h"
#include <WebCore/PlatformPasteboard.h>
#include <WebCore/SharedBuffer.h>
#include <wtf/SetForScope.h>

namespace WebKit {
using namespace WebCore;

void WebPasteboardProxy::getTypes(const String& pasteboardName, CompletionHandler<void(Vector<String>&&)>&& completionHandler)
{
    Clipboard::get(pasteboardName).formats(WTFMove(completionHandler));
}

void WebPasteboardProxy::readText(const String& pasteboardName, CompletionHandler<void(String&&)>&& completionHandler)
{
    Clipboard::get(pasteboardName).readText(WTFMove(completionHandler));
}

void WebPasteboardProxy::readFilePaths(const String& pasteboardName, CompletionHandler<void(Vector<String>&&)>&& completionHandler)
{
    Clipboard::get(pasteboardName).readFilePaths(WTFMove(completionHandler));
}

void WebPasteboardProxy::readBuffer(const String& pasteboardName, const String& pasteboardType, CompletionHandler<void(IPC::SharedBufferDataReference&&)>&& completionHandler)
{
    Clipboard::get(pasteboardName).readBuffer(pasteboardType.utf8().data(), WTFMove(completionHandler));
}

void WebPasteboardProxy::writeToClipboard(const String& pasteboardName, WebSelectionData&& selection)
{
    Clipboard::get(pasteboardName).write(WTFMove(selection.selectionData));
}

void WebPasteboardProxy::clearClipboard(const String& pasteboardName)
{
    Clipboard::get(pasteboardName).clear();
}

void WebPasteboardProxy::setPrimarySelectionOwner(WebFrameProxy* frame)
{
    if (m_primarySelectionOwner == frame)
        return;

    if (m_primarySelectionOwner)
        m_primarySelectionOwner->collapseSelection();

    m_primarySelectionOwner = frame;
}

void WebPasteboardProxy::didDestroyFrame(WebFrameProxy* frame)
{
    if (frame == m_primarySelectionOwner)
        m_primarySelectionOwner = nullptr;
}

} // namespace WebKit
