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

#ifndef FontFaceSet_h
#define FontFaceSet_h

#include "ActiveDOMObject.h"
#include "CSSFontFaceSet.h"
#include "DOMCoreException.h"
#include "EventTarget.h"
#include "JSDOMPromise.h"
#include <wtf/HashTraits.h>
#include <wtf/Optional.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace JSC {
class ExecState;
}

namespace WebCore {

class Document;
class FontFace;

class FontFaceSet final : public RefCounted<FontFaceSet>, public CSSFontFaceSetClient, public EventTargetWithInlineData, public ActiveDOMObject {
public:
    static Ref<FontFaceSet> create(Document&, const Vector<RefPtr<FontFace>>& initialFaces);
    static Ref<FontFaceSet> create(Document&, CSSFontFaceSet& backing);
    virtual ~FontFaceSet();

    bool has(RefPtr<WebCore::FontFace>) const;
    size_t size() const;
    FontFaceSet& add(RefPtr<WebCore::FontFace>);
    bool remove(RefPtr<WebCore::FontFace>);
    void clear();

    void load(JSC::ExecState& execState, const String& font, DeferredWrapper&& promise, ExceptionCode& ec) { load(execState, font, String(" ", String::ConstructFromLiteral), WTFMove(promise), ec); }
    void load(JSC::ExecState&, const String& font, const String& text, DeferredWrapper&& promise, ExceptionCode&);
    bool check(const String& font, ExceptionCode& ec) { return check(font, String(" ", String::ConstructFromLiteral), ec); }
    bool check(const String& font, const String& text, ExceptionCode&);

    String status() const;

    typedef DOMPromise<FontFaceSet&, DOMCoreException&> Promise;
    Promise& promise(JSC::ExecState&);

    CSSFontFaceSet& backing() { return m_backing; }

    class Iterator {
    public:
        explicit Iterator(FontFaceSet&);
        Optional<WTF::KeyValuePair<RefPtr<FontFace>, RefPtr<FontFace>>> next(JSC::ExecState&);

    private:
        Ref<FontFaceSet> m_target;
        size_t m_index { 0 }; // FIXME: There needs to be a mechanism to handle when fonts are added or removed from the middle of the FontFaceSet.
    };
    Iterator createIterator() { return Iterator(*this); }

    using RefCounted<FontFaceSet>::ref;
    using RefCounted<FontFaceSet>::deref;

private:
    struct PendingPromise : public RefCounted<PendingPromise> {
        typedef DOMPromise<Vector<RefPtr<FontFace>>&, DOMCoreException&> Promise;
        static Ref<PendingPromise> create(Promise&& promise)
        {
            return adoptRef(*new PendingPromise(WTFMove(promise)));
        }
        ~PendingPromise();

    private:
        PendingPromise(Promise&&);

    public:
        Vector<RefPtr<FontFace>> faces;
        Promise promise;
    };

    FontFaceSet(Document&, const Vector<RefPtr<FontFace>>&);
    FontFaceSet(Document&, CSSFontFaceSet&);

    void fulfillPromise();

    // CSSFontFaceSetClient
    void startedLoading() override;
    void completedLoading() override;
    void faceFinished(CSSFontFace&, CSSFontFace::Status) override;

    // ActiveDOMObject
    const char* activeDOMObjectName() const override { return "FontFaceSet"; }
    bool canSuspendForDocumentSuspension() const override;

    // EventTarget
    EventTargetInterface eventTargetInterface() const override { return FontFaceSetEventTargetInterfaceType; }
    ScriptExecutionContext* scriptExecutionContext() const override { return ActiveDOMObject::scriptExecutionContext(); }
    void refEventTarget() override { ref(); }
    void derefEventTarget() override { deref(); }

    Ref<CSSFontFaceSet> m_backing;
    HashMap<RefPtr<CSSFontFace>, Vector<Ref<PendingPromise>>> m_pendingPromises;
    Optional<Promise> m_promise;
};

}

#endif
