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

#ifndef FontFace_h
#define FontFace_h

#include "CSSFontFace.h"
#include "CSSPropertyNames.h"
#include "DOMCoreException.h"
#include "ExceptionCode.h"
#include "JSDOMPromise.h"
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

namespace Deprecated {
class ScriptValue;
}

namespace WebCore {

class CSSFontFace;
class CSSValue;
class Dictionary;

class FontFace final : public RefCounted<FontFace>, public CSSFontFace::Client {
public:
    static RefPtr<FontFace> create(JSC::ExecState&, ScriptExecutionContext&, const String& family, const Deprecated::ScriptValue& source, const Dictionary& descriptors, ExceptionCode&);
    static Ref<FontFace> create(JSC::ExecState&, CSSFontFace&);
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
    String status() const;

    typedef DOMPromise<FontFace&, DOMCoreException&> Promise;
    Promise& promise() { return m_promise; }

    void load();

    CSSFontFace& backing() { return m_backing; }

    static RefPtr<CSSValue> parseString(const String&, CSSPropertyID);

    void fontStateChanged(CSSFontFace&, CSSFontFace::Status oldState, CSSFontFace::Status newState) override;

    WeakPtr<FontFace> createWeakPtr() const;

    // CSSFontFace::Client needs to be able to be held in a RefPtr.
    void ref() override { RefCounted<FontFace>::ref(); }
    void deref() override { RefCounted<FontFace>::deref(); }

private:
    FontFace(JSC::ExecState&, CSSFontSelector&);
    FontFace(JSC::ExecState&, CSSFontFace&);

    void fulfillPromise();
    void rejectPromise(ExceptionCode);

    WeakPtrFactory<FontFace> m_weakPtrFactory;
    Ref<CSSFontFace> m_backing;
    Promise m_promise;
};

}

#endif
