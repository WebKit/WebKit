/*
 * Copyright (C) 2007-2025 Apple Inc. All rights reserved.
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
#include "CSSFontFace.h"
#include "CSSPropertyNames.h"
#include "IDLTypes.h"
#include <wtf/UniqueRef.h>
#include <wtf/WeakPtr.h>

namespace JSC {
class ArrayBuffer;
class ArrayBufferView;
}

namespace WebCore {

template<typename IDLType> class DOMPromiseProxyWithResolveCallback;
template<typename> class ExceptionOr;

class FontFace final : public RefCounted<FontFace>, public ActiveDOMObject, public CSSFontFaceClient {
public:
    struct Descriptors {
        RefPtr<StringImpl> style;
        RefPtr<StringImpl> weight;
        RefPtr<StringImpl> width;
        RefPtr<StringImpl> unicodeRange;
        RefPtr<StringImpl> featureSettings;
        RefPtr<StringImpl> display;
        RefPtr<StringImpl> ascentOverride;
        RefPtr<StringImpl> descentOverride;
        RefPtr<StringImpl> lineGapOverride;
        RefPtr<StringImpl> sizeAdjust;
    };
    
    using Source = Variant<RefPtr<StringImpl>, Ref<JSC::ArrayBuffer>, Ref<JSC::ArrayBufferView>>;
    static Ref<FontFace> create(ScriptExecutionContext&, AtomStringImpl* family, Source&&, const Descriptors&);
    static Ref<FontFace> create(ScriptExecutionContext*, CSSFontFace&);
    virtual ~FontFace();

    // ContextDestructionObserver.
    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }
    USING_CAN_MAKE_WEAKPTR(CSSFontFaceClient);

    ExceptionOr<void> setFamily(ScriptExecutionContext&, AtomStringImpl*);
    ExceptionOr<void> setStyle(ScriptExecutionContext&, StringImpl*);
    ExceptionOr<void> setWeight(ScriptExecutionContext&, StringImpl*);
    ExceptionOr<void> setWidth(ScriptExecutionContext&, StringImpl*);
    ExceptionOr<void> setUnicodeRange(ScriptExecutionContext&, StringImpl*);
    ExceptionOr<void> setFeatureSettings(ScriptExecutionContext&, StringImpl*);
    ExceptionOr<void> setDisplay(ScriptExecutionContext&, StringImpl*);
    ExceptionOr<void> setAscentOverride(ScriptExecutionContext&, StringImpl*);
    ExceptionOr<void> setDescentOverride(ScriptExecutionContext&, StringImpl*);
    ExceptionOr<void> setLineGapOverride(ScriptExecutionContext&, StringImpl*);
    ExceptionOr<void> setSizeAdjust(ScriptExecutionContext&, StringImpl*);

    RefPtr<AtomStringImpl> family() const;
    RefPtr<StringImpl> style() const;
    RefPtr<StringImpl> weight() const;
    RefPtr<StringImpl> width() const;
    RefPtr<StringImpl> unicodeRange() const;
    RefPtr<StringImpl> featureSettings() const;
    RefPtr<StringImpl> display() const;
    RefPtr<StringImpl> ascentOverride() const;
    RefPtr<StringImpl> descentOverride() const;
    RefPtr<StringImpl> lineGapOverride() const;
    RefPtr<StringImpl> sizeAdjust() const;

    enum class LoadStatus { Unloaded, Loading, Loaded, Error };
    NODELETE LoadStatus status() const;

    using LoadedPromise = DOMPromiseProxyWithResolveCallback<IDLInterface<FontFace>>;
    NODELETE LoadedPromise& loadedForBindings();
    LoadedPromise& loadForBindings();

    void adopt(CSSFontFace&);

    CSSFontFace& backing() { return m_backing; }

    void fontStateChanged(CSSFontFace&, CSSFontFace::Status oldState, CSSFontFace::Status newState) final;

private:
    explicit FontFace(CSSFontSelector&);
    explicit FontFace(ScriptExecutionContext*, CSSFontFace&);

    // ActiveDOMObject.
    NODELETE bool virtualHasPendingActivity() const final;

    // Callback for LoadedPromise.
    NODELETE FontFace& loadedPromiseResolve();
    void setErrorState();

    Ref<CSSFontFace> m_backing;
    const UniqueRef<LoadedPromise> m_loadedPromise;
    bool m_mayLoadedPromiseBeScriptObservable { false };
    bool m_sourceIsImmediateBuffer { false };
};

}
