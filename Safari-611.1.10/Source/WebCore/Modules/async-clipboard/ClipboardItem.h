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

#pragma once

#include <wtf/KeyValuePair.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class Blob;
class Clipboard;
class ClipboardItemDataSource;
class DeferredPromise;
class DOMPromise;
class Navigator;
class PasteboardCustomData;
class ScriptExecutionContext;
struct PasteboardItemInfo;

class ClipboardItem : public RefCounted<ClipboardItem> {
public:
    ~ClipboardItem();

    enum class PresentationStyle : uint8_t { Unspecified, Inline, Attachment };

    struct Options {
        PresentationStyle presentationStyle { PresentationStyle::Unspecified };
    };

    static Ref<ClipboardItem> create(Vector<KeyValuePair<String, RefPtr<DOMPromise>>>&&, const Options&);
    static Ref<ClipboardItem> create(Clipboard&, const PasteboardItemInfo&);
    static Ref<Blob> blobFromString(ScriptExecutionContext*, const String& stringData, const String& type);

    Vector<String> types() const;
    void getType(const String&, Ref<DeferredPromise>&&);

    void collectDataForWriting(Clipboard& destination, CompletionHandler<void(Optional<PasteboardCustomData>)>&&);

    PresentationStyle presentationStyle() const { return m_presentationStyle; };
    Navigator* navigator();
    Clipboard* clipboard();

private:
    ClipboardItem(Vector<KeyValuePair<String, RefPtr<DOMPromise>>>&&, const Options&);
    ClipboardItem(Clipboard&, const PasteboardItemInfo&);

    WeakPtr<Clipboard> m_clipboard;
    WeakPtr<Navigator> m_navigator;
    std::unique_ptr<ClipboardItemDataSource> m_dataSource;
    PresentationStyle m_presentationStyle { PresentationStyle::Unspecified };
};

} // namespace WebCore
