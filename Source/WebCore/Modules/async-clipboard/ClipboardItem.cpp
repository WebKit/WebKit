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
#include "ClipboardItem.h"

#include "Blob.h"
#include "Clipboard.h"
#include "ClipboardItemBindingsDataSource.h"
#include "ClipboardItemPasteboardDataSource.h"
#include "Navigator.h"
#include "PasteboardCustomData.h"
#include "PasteboardItemInfo.h"
#include "SharedBuffer.h"

namespace WebCore {

ClipboardItem::~ClipboardItem() = default;

Ref<Blob> ClipboardItem::blobFromString(const String& stringData, const String& type)
{
    auto utf8 = stringData.utf8();
    return Blob::create(SharedBuffer::create(utf8.data(), utf8.length()), Blob::normalizedContentType(type));
}

static ClipboardItem::PresentationStyle clipboardItemPresentationStyle(const PasteboardItemInfo& info)
{
    switch (info.preferredPresentationStyle) {
    case PasteboardItemPresentationStyle::Unspecified:
        return ClipboardItem::PresentationStyle::Unspecified;
    case PasteboardItemPresentationStyle::Inline:
        return ClipboardItem::PresentationStyle::Inline;
    case PasteboardItemPresentationStyle::Attachment:
        return ClipboardItem::PresentationStyle::Attachment;
    }
    ASSERT_NOT_REACHED();
    return ClipboardItem::PresentationStyle::Unspecified;
}

ClipboardItem::ClipboardItem(Vector<KeyValuePair<String, RefPtr<DOMPromise>>>&& items, const Options& options)
    : m_dataSource(makeUnique<ClipboardItemBindingsDataSource>(*this, WTFMove(items)))
    , m_presentationStyle(options.presentationStyle)
{
}

ClipboardItem::ClipboardItem(Clipboard& clipboard, const PasteboardItemInfo& info)
    : m_clipboard(makeWeakPtr(clipboard))
    , m_navigator(makeWeakPtr(clipboard.navigator()))
    , m_dataSource(makeUnique<ClipboardItemPasteboardDataSource>(*this, info))
    , m_presentationStyle(clipboardItemPresentationStyle(info))
{
}

Ref<ClipboardItem> ClipboardItem::create(Vector<KeyValuePair<String, RefPtr<DOMPromise>>>&& data, const Options& options)
{
    return adoptRef(*new ClipboardItem(WTFMove(data), options));
}

Ref<ClipboardItem> ClipboardItem::create(Clipboard& clipboard, const PasteboardItemInfo& info)
{
    return adoptRef(*new ClipboardItem(clipboard, info));
}

Vector<String> ClipboardItem::types() const
{
    return m_dataSource->types();
}

void ClipboardItem::getType(const String& type, Ref<DeferredPromise>&& promise)
{
    m_dataSource->getType(type, WTFMove(promise));
}

void ClipboardItem::collectDataForWriting(Clipboard& destination, CompletionHandler<void(Optional<PasteboardCustomData>)>&& completion)
{
    m_dataSource->collectDataForWriting(destination, WTFMove(completion));
}

Navigator* ClipboardItem::navigator()
{
    return m_navigator.get();
}

Clipboard* ClipboardItem::clipboard()
{
    return m_clipboard.get();
}

} // namespace WebCore
