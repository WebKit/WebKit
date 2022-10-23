/*
 * Copyright (C) 2007, 2008, 2016 Apple Inc. All rights reserved.
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
#include "ExceptionOr.h"
#include "IDLTypes.h"
#include <variant>
#include <wtf/UniqueRef.h>
#include <wtf/WeakPtr.h>

namespace JSC {
class ArrayBuffer;
class ArrayBufferView;
}

namespace WebCore {

template<typename IDLType> class DOMPromiseProxyWithResolveCallback;

class FontFace final : public RefCounted<FontFace>, public CanMakeWeakPtr<FontFace>, public ActiveDOMObject, private CSSFontFace::Client {
public:
    struct Descriptors {
        String style;
        String weight;
        String stretch;
        String unicodeRange;
        String featureSettings;
        String display;
    };
    
    using Source = std::variant<String, RefPtr<JSC::ArrayBuffer>, RefPtr<JSC::ArrayBufferView>>;
    static Ref<FontFace> create(ScriptExecutionContext&, const String& family, Source&&, const Descriptors&);
    static Ref<FontFace> create(ScriptExecutionContext*, CSSFontFace&);
    virtual ~FontFace();

    ExceptionOr<void> setFamily(ScriptExecutionContext&, const String&);
    ExceptionOr<void> setStyle(ScriptExecutionContext&, const String&);
    ExceptionOr<void> setWeight(ScriptExecutionContext&, const String&);
    ExceptionOr<void> setStretch(ScriptExecutionContext&, const String&);
    ExceptionOr<void> setUnicodeRange(ScriptExecutionContext&, const String&);
    ExceptionOr<void> setFeatureSettings(ScriptExecutionContext&, const String&);
    ExceptionOr<void> setDisplay(ScriptExecutionContext&, const String&);

    String family() const;
    String style() const;
    String weight() const;
    String stretch() const;
    String unicodeRange() const;
    String featureSettings() const;
    String display() const;

    enum class LoadStatus { Unloaded, Loading, Loaded, Error };
    LoadStatus status() const;

    using LoadedPromise = DOMPromiseProxyWithResolveCallback<IDLInterface<FontFace>>;
    LoadedPromise& loadedForBindings();
    LoadedPromise& loadForBindings();

    void adopt(CSSFontFace&);

    CSSFontFace& backing() { return m_backing; }

    void fontStateChanged(CSSFontFace&, CSSFontFace::Status oldState, CSSFontFace::Status newState) final;

    void ref() final { RefCounted::ref(); }
    void deref() final { RefCounted::deref(); }

private:
    explicit FontFace(CSSFontSelector&);
    explicit FontFace(ScriptExecutionContext*, CSSFontFace&);

    // ActiveDOMObject.
    const char* activeDOMObjectName() const final;
    bool virtualHasPendingActivity() const final;

    // Callback for LoadedPromise.
    FontFace& loadedPromiseResolve();
    void setErrorState();

    Ref<CSSFontFace> m_backing;
    UniqueRef<LoadedPromise> m_loadedPromise;
    bool m_mayLoadedPromiseBeScriptObservable { false };
};

}
