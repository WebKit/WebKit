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

#include "ClipboardItemDataSource.h"
#include "ExceptionCode.h"
#include "FileReaderLoaderClient.h"
#include <variant>
#include <wtf/WeakPtr.h>

namespace WebCore {

class Blob;
class SharedBuffer;
class DOMPromise;
class WeakPtrImplWithEventTargetData;
class FileReaderLoader;
class PasteboardCustomData;
class ScriptExecutionContext;

class ClipboardItemBindingsDataSource : public ClipboardItemDataSource {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ClipboardItemBindingsDataSource(ClipboardItem&, Vector<KeyValuePair<String, RefPtr<DOMPromise>>>&&);
    ~ClipboardItemBindingsDataSource();

private:
    Vector<String> types() const final;
    void getType(const String&, Ref<DeferredPromise>&&) final;
    void collectDataForWriting(Clipboard& destination, CompletionHandler<void(std::optional<PasteboardCustomData>)>&&) final;

    void invokeCompletionHandler();

    using BufferOrString = std::variant<String, Ref<SharedBuffer>>;
    class ClipboardItemTypeLoader : public FileReaderLoaderClient, public RefCounted<ClipboardItemTypeLoader> {
    public:
        static Ref<ClipboardItemTypeLoader> create(Clipboard& writingDestination, const String& type, CompletionHandler<void()>&& completionHandler)
        {
            return adoptRef(*new ClipboardItemTypeLoader(writingDestination, type, WTFMove(completionHandler)));
        }

        ~ClipboardItemTypeLoader();

        void didResolveToString(const String&);
        void didFailToResolve();
        void didResolveToBlob(ScriptExecutionContext&, Ref<Blob>&&);

        const String& type() { return m_type; }
        const BufferOrString& data() { return m_data; }

    private:
        ClipboardItemTypeLoader(Clipboard&, const String& type, CompletionHandler<void()>&&);

        void sanitizeDataIfNeeded();
        void invokeCompletionHandler();

        String dataAsString() const;

        // FileReaderLoaderClient methods.
        void didStartLoading() final { }
        void didReceiveData() final { }
        void didFinishLoading() final;
        void didFail(ExceptionCode) final;

        String m_type;
        BufferOrString m_data;
        std::unique_ptr<FileReaderLoader> m_blobLoader;
        CompletionHandler<void()> m_completionHandler;
        WeakPtr<Clipboard, WeakPtrImplWithEventTargetData> m_writingDestination;
    };

    unsigned m_numberOfPendingClipboardTypes { 0 };
    CompletionHandler<void(std::optional<PasteboardCustomData>)> m_completionHandler;
    Vector<Ref<ClipboardItemTypeLoader>> m_itemTypeLoaders;
    WeakPtr<Clipboard, WeakPtrImplWithEventTargetData> m_writingDestination;

    Vector<KeyValuePair<String, RefPtr<DOMPromise>>> m_itemPromises;
};

} // namespace WebCore
