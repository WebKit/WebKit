/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "ActiveDOMObject.h"
#include "CSSFontFaceSet.h"
#include "DOMPromiseProxy.h"
#include "EventTarget.h"
#include "JSDOMPromiseDeferred.h"

namespace WebCore {

class DOMException;

class FontFaceSet final : public RefCounted<FontFaceSet>, private CSSFontFaceSetClient, public EventTargetWithInlineData, private  ActiveDOMObject {
    WTF_MAKE_ISO_ALLOCATED(FontFaceSet);
public:
    static Ref<FontFaceSet> create(Document&, const Vector<RefPtr<FontFace>>& initialFaces);
    static Ref<FontFaceSet> create(Document&, CSSFontFaceSet& backing);
    virtual ~FontFaceSet();

    bool has(FontFace&) const;
    size_t size() const;
    FontFaceSet& add(FontFace&);
    bool remove(FontFace&);
    void clear();

    using LoadPromise = DOMPromiseDeferred<IDLSequence<IDLInterface<FontFace>>>;
    void load(const String& font, const String& text, LoadPromise&&);
    ExceptionOr<bool> check(const String& font, const String& text);

    enum class LoadStatus { Loading, Loaded };
    LoadStatus status() const;

    using ReadyPromise = DOMPromiseProxyWithResolveCallback<IDLInterface<FontFaceSet>>;
    ReadyPromise& ready() { return m_readyPromise; }
    void didFirstLayout();

    CSSFontFaceSet& backing() { return m_backing; }

    class Iterator {
    public:
        explicit Iterator(FontFaceSet&);
        RefPtr<FontFace> next();

    private:
        Ref<FontFaceSet> m_target;
        size_t m_index { 0 }; // FIXME: There needs to be a mechanism to handle when fonts are added or removed from the middle of the FontFaceSet.
    };
    Iterator createIterator() { return Iterator(*this); }

    using RefCounted::ref;
    using RefCounted::deref;

private:
    struct PendingPromise : RefCounted<PendingPromise> {
        static Ref<PendingPromise> create(LoadPromise&& promise)
        {
            return adoptRef(*new PendingPromise(WTFMove(promise)));
        }
        ~PendingPromise();

    private:
        PendingPromise(LoadPromise&&);

    public:
        Vector<Ref<FontFace>> faces;
        LoadPromise promise;
        bool hasReachedTerminalState { false };
    };

    FontFaceSet(Document&, const Vector<RefPtr<FontFace>>&);
    FontFaceSet(Document&, CSSFontFaceSet&);

    // CSSFontFaceSetClient
    void startedLoading() final;
    void completedLoading() final;
    void faceFinished(CSSFontFace&, CSSFontFace::Status) final;

    // ActiveDOMObject
    const char* activeDOMObjectName() const final { return "FontFaceSet"; }
    bool canSuspendForDocumentSuspension() const final;

    // EventTarget
    EventTargetInterface eventTargetInterface() const final { return FontFaceSetEventTargetInterfaceType; }
    ScriptExecutionContext* scriptExecutionContext() const final { return ActiveDOMObject::scriptExecutionContext(); }
    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }

    // Callback for ReadyPromise.
    FontFaceSet& readyPromiseResolve();

    Ref<CSSFontFaceSet> m_backing;
    HashMap<RefPtr<FontFace>, Vector<Ref<PendingPromise>>> m_pendingPromises;
    ReadyPromise m_readyPromise;
    bool m_isFirstLayoutDone { true };
};

}
