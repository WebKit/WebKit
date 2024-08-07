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

#include "EventTarget.h"
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class ClipboardItem;
class DeferredPromise;
class LocalFrame;
class Navigator;
class Pasteboard;
class PasteboardCustomData;

class Clipboard final : public RefCounted<Clipboard>, public EventTarget {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(Clipboard);
public:
    static Ref<Clipboard> create(Navigator&);
    ~Clipboard();

    enum EventTargetInterfaceType eventTargetInterface() const final;
    ScriptExecutionContext* scriptExecutionContext() const final;

    LocalFrame* frame() const;
    Navigator* navigator();

    using RefCounted::ref;
    using RefCounted::deref;

    void readText(Ref<DeferredPromise>&&);
    void writeText(const String& data, Ref<DeferredPromise>&&);

    void read(Ref<DeferredPromise>&&);
    void write(const Vector<Ref<ClipboardItem>>& data, Ref<DeferredPromise>&&);

    void getType(ClipboardItem&, const String& type, Ref<DeferredPromise>&&);

private:
    Clipboard(Navigator&);

    struct Session {
        std::unique_ptr<Pasteboard> pasteboard;
        Vector<Ref<ClipboardItem>> items;
        int64_t changeCount;
    };

    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }

    Pasteboard& activePasteboard();

    enum class SessionIsValid : bool { No, Yes };
    SessionIsValid updateSessionValidity();

    class ItemWriter : public RefCounted<ItemWriter> {
    public:
        static Ref<ItemWriter> create(Clipboard& clipboard, Ref<DeferredPromise>&& promise)
        {
            return adoptRef(*new ItemWriter(clipboard, WTFMove(promise)));
        }

        ~ItemWriter();

        void write(const Vector<Ref<ClipboardItem>>&);
        void invalidate();

    private:
        ItemWriter(Clipboard&, Ref<DeferredPromise>&&);

        void setData(std::optional<PasteboardCustomData>&&, size_t index);
        void didSetAllData();
        void reject();

        WeakPtr<Clipboard, WeakPtrImplWithEventTargetData> m_clipboard;
        Vector<std::optional<PasteboardCustomData>> m_dataToWrite;
        RefPtr<DeferredPromise> m_promise;
        unsigned m_pendingItemCount;
        std::unique_ptr<Pasteboard> m_pasteboard;
        int64_t m_changeCountAtStart { 0 };
    };

    void didResolveOrReject(ItemWriter&);

    std::optional<Session> m_activeSession;
    WeakPtr<Navigator> m_navigator;
    Vector<std::optional<PasteboardCustomData>> m_dataToWrite;
    RefPtr<ItemWriter> m_activeItemWriter;
};

} // namespace WebCore
