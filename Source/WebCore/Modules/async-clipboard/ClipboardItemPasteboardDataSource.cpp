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
#include "ClipboardItemPasteboardDataSource.h"

#include "Clipboard.h"
#include "ClipboardItem.h"
#include "JSDOMPromiseDeferred.h"
#include "PasteboardCustomData.h"
#include "PasteboardItemInfo.h"

namespace WebCore {

ClipboardItemPasteboardDataSource::ClipboardItemPasteboardDataSource(ClipboardItem& item, const PasteboardItemInfo& info)
    : ClipboardItemDataSource(item)
    , m_types(info.webSafeTypesByFidelity)
{
}

ClipboardItemPasteboardDataSource::~ClipboardItemPasteboardDataSource() = default;

Vector<String> ClipboardItemPasteboardDataSource::types() const
{
    return m_types;
}

void ClipboardItemPasteboardDataSource::getType(const String& type, Ref<DeferredPromise>&& promise)
{
    if (RefPtr clipboard = m_item.clipboard())
        clipboard->getType(m_item, type, WTFMove(promise));
    else
        promise->reject(ExceptionCode::NotAllowedError);
}

void ClipboardItemPasteboardDataSource::collectDataForWriting(Clipboard&, CompletionHandler<void(std::optional<PasteboardCustomData>)>&& completion)
{
    // FIXME: Not implemented. This is needed to support writing platform-backed ClipboardItems
    // back to the pasteboard using Clipboard.write().
    completion(std::nullopt);
}

} // namespace WebCore
