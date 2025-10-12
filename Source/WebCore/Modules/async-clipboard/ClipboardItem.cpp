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
#include "CommonAtomStrings.h"
#include "ContextDestructionObserverInlines.h"
#include "ExceptionOr.h"
#include "Navigator.h"
#include "PasteboardCustomData.h"
#include "PasteboardItemInfo.h"
#include "SharedBuffer.h"

namespace WebCore {

ClipboardItem::~ClipboardItem() = default;

Ref<Blob> ClipboardItem::blobFromString(ScriptExecutionContext* context, const String& stringData, const String& type)
{
    return Blob::create(context, Vector(byteCast<uint8_t>(stringData.utf8().span())), Blob::normalizedContentType(type));
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

// FIXME: Custom format starts with `"web "`("web" followed by U+0020 SPACE) prefix
// and suffix (after stripping out `"web "`) passes the parsing a MIME type check.
// https://w3c.github.io/clipboard-apis/#optional-data-types
// https://webkit.org/b/280664
ClipboardItem::ClipboardItem(Vector<KeyValuePair<String, Ref<DOMPromise>>>&& items, const Options& options)
    : m_dataSource(makeUniqueRef<ClipboardItemBindingsDataSource>(*this, WTFMove(items)))
    , m_presentationStyle(options.presentationStyle)
{
}

ClipboardItem::ClipboardItem(Clipboard& clipboard, const PasteboardItemInfo& info)
    : m_clipboard(clipboard)
    , m_navigator(clipboard.navigator())
    , m_dataSource(makeUniqueRef<ClipboardItemPasteboardDataSource>(*this, info))
    , m_presentationStyle(clipboardItemPresentationStyle(info))
{
}

ExceptionOr<Ref<ClipboardItem>> ClipboardItem::create(Vector<KeyValuePair<String, Ref<DOMPromise>>>&& data, const Options& options)
{
    if (data.isEmpty())
        return Exception { ExceptionCode::TypeError, "ClipboardItem() can not be an empty array: {}"_s };
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

bool ClipboardItem::supports(const String& type)
{
    // FIXME: Custom format starts with `"web "`("web" followed by U+0020 SPACE) prefix
    // and suffix (after stripping out `"web "`) passes the parsing a MIME type check.
    // https://webkit.org/b/280664
    // FIXME: add type == "image/svg+xml"_s when we have sanitized copy/paste for SVG data
    // https://webkit.org/b/280726
    if (type == textPlainContentTypeAtom()
        || type == textHTMLContentTypeAtom()
        || type == "image/png"_s
        || type == "text/uri-list"_s) {
        return true;
        }
    return false;
}

void ClipboardItem::collectDataForWriting(Clipboard& destination, CompletionHandler<void(std::optional<PasteboardCustomData>)>&& completion)
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
