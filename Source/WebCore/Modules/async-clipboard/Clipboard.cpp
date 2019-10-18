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

#include "config.h"
#include "Clipboard.h"

#include "Blob.h"
#include "ClipboardItem.h"
#include "Frame.h"
#include "JSClipboardItem.h"
#include "JSDOMPromise.h"
#include "JSDOMPromiseDeferred.h"
#include "Navigator.h"
#include "Pasteboard.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(Clipboard);

Ref<Clipboard> Clipboard::create(Navigator& navigator)
{
    return adoptRef(*new Clipboard(navigator));
}

Clipboard::Clipboard(Navigator& navigator)
    : m_navigator(makeWeakPtr(navigator))
{
}

Clipboard::~Clipboard() = default;

Navigator* Clipboard::navigator()
{
    return m_navigator.get();
}

EventTargetInterface Clipboard::eventTargetInterface() const
{
    return ClipboardEventTargetInterfaceType;
}

ScriptExecutionContext* Clipboard::scriptExecutionContext() const
{
    return m_navigator ? m_navigator->scriptExecutionContext() : nullptr;
}

void Clipboard::readText(Ref<DeferredPromise>&& promise)
{
    promise->reject(NotSupportedError);
}

void Clipboard::writeText(const String& data, Ref<DeferredPromise>&& promise)
{
    UNUSED_PARAM(data);
    promise->reject(NotSupportedError);
}

void Clipboard::read(Ref<DeferredPromise>&& promise)
{
    auto rejectPromiseAndClearActiveSession = [&] {
        m_activeSession = WTF::nullopt;
        promise->reject(NotAllowedError);
    };

    auto frame = makeRefPtr(this->frame());
    if (!frame) {
        rejectPromiseAndClearActiveSession();
        return;
    }

    auto pasteboard = Pasteboard::createForCopyAndPaste();
    int changeCountAtStart = pasteboard->changeCount();

    if (!frame->requestDOMPasteAccess()) {
        rejectPromiseAndClearActiveSession();
        return;
    }

    if (!m_activeSession || m_activeSession->changeCount != changeCountAtStart) {
        auto allInfo = pasteboard->allPasteboardItemInfo();
        if (allInfo.isEmpty()) {
            rejectPromiseAndClearActiveSession();
            return;
        }

        Vector<Ref<ClipboardItem>> clipboardItems;
        clipboardItems.reserveInitialCapacity(allInfo.size());
        for (auto& itemInfo : allInfo) {
            // FIXME: This should be refactored such that the initial changeCount is delivered to the client, where it is then checked
            // against the current changeCount of the platform pasteboard. For instance, in WebKit2, this would relocate the changeCount
            // check to the UI process instead of the web content process.
            if (itemInfo.changeCount != changeCountAtStart) {
                rejectPromiseAndClearActiveSession();
                return;
            }
            clipboardItems.uncheckedAppend(ClipboardItem::create(*this, itemInfo));
        }
        m_activeSession = {{ WTFMove(pasteboard), WTFMove(clipboardItems), changeCountAtStart }};
    }

    promise->resolve<IDLSequence<IDLInterface<ClipboardItem>>>(m_activeSession->items);
}

void Clipboard::getType(ClipboardItem& item, const String& type, Ref<DeferredPromise>&& promise)
{
    UNUSED_PARAM(item);
    UNUSED_PARAM(type);
    promise->reject(NotSupportedError);
}

void Clipboard::write(const Vector<RefPtr<ClipboardItem>>& items, Ref<DeferredPromise>&& promise)
{
    UNUSED_PARAM(items);
    promise->reject(NotSupportedError);
}

Frame* Clipboard::frame() const
{
    return m_navigator ? m_navigator->frame() : nullptr;
}

}
