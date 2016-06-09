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

#include "CSSFontFace.h"
#include "CSSPropertyNames.h"
#include "JSDOMPromise.h"
#include <wtf/WeakPtr.h>

namespace WebCore {

class FontFace final : public RefCounted<FontFace>, private CSSFontFace::Client {
public:
    struct Descriptors {
        String style;
        String weight;
        String stretch;
        String unicodeRange;
        String variant;
        String featureSettings;
    };
    static RefPtr<FontFace> create(JSC::ExecState&, Document&, const String& family, JSC::JSValue source, const Descriptors&, ExceptionCode&);
    static Ref<FontFace> create(CSSFontFace&);
    virtual ~FontFace();

    void setFamily(const String&, ExceptionCode&);
    void setStyle(const String&, ExceptionCode&);
    void setWeight(const String&, ExceptionCode&);
    void setStretch(const String&, ExceptionCode&);
    void setUnicodeRange(const String&, ExceptionCode&);
    void setVariant(const String&, ExceptionCode&);
    void setFeatureSettings(const String&, ExceptionCode&);

    String family() const;
    String style() const;
    String weight() const;
    String stretch() const;
    String unicodeRange() const;
    String variant() const;
    String featureSettings() const;

    enum class LoadStatus { Unloaded, Loading, Loaded, Error };
    LoadStatus status() const;

    typedef DOMPromise<FontFace&> Promise;
    Optional<Promise>& promise() { return m_promise; }
    void registerLoaded(Promise&&);

    void adopt(CSSFontFace&);

    void load();

    CSSFontFace& backing() { return m_backing; }

    static RefPtr<CSSValue> parseString(const String&, CSSPropertyID);

    void fontStateChanged(CSSFontFace&, CSSFontFace::Status oldState, CSSFontFace::Status newState) final;

    WeakPtr<FontFace> createWeakPtr() const;

    void ref() final { RefCounted::ref(); }
    void deref() final { RefCounted::deref(); }

private:
    explicit FontFace(CSSFontSelector&);
    explicit FontFace(CSSFontFace&);

    WeakPtrFactory<FontFace> m_weakPtrFactory;
    Ref<CSSFontFace> m_backing;
    Optional<Promise> m_promise;
};

}
