/*
 * Copyright (C) 2007-2021 Apple Inc. All rights reserved.
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

#include "FontLoadRequest.h"
#include <JavaScriptCore/ArrayBufferView.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/AtomString.h>

namespace WebCore {

class CSSFontFace;
class CSSFontSelector;
class SharedBuffer;
class Document;
class Font;
class FontCreationContext;
struct FontCustomPlatformData;
class FontDescription;
struct FontSelectionSpecifiedCapabilities;
struct FontVariantSettings;
class SVGFontFaceElement;

template <typename T> class FontTaggedSettings;
typedef FontTaggedSettings<int> FontFeatureSettings;

class CSSFontFaceSource final : public FontLoadRequestClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    CSSFontFaceSource(CSSFontFace& owner, const String& familyNameOrURI);
    CSSFontFaceSource(CSSFontFace& owner, const String& familyNameOrURI, CSSFontSelector&, UniqueRef<FontLoadRequest>&&);
    CSSFontFaceSource(CSSFontFace& owner, const String& familyNameOrURI, SVGFontFaceElement&);
    CSSFontFaceSource(CSSFontFace& owner, const String& familyNameOrURI, Ref<JSC::ArrayBufferView>&&);
    virtual ~CSSFontFaceSource();

    //                      => Success
    //                    //
    // Pending => Loading
    //                    \\.
    //                      => Failure
    enum class Status {
        Pending,
        Loading,
        Success,
        Failure
    };
    Status status() const { return m_status; }

    const AtomString& familyNameOrURI() const { return m_familyNameOrURI; }

    void opportunisticallyStartFontDataURLLoading();

    void load(Document* = nullptr);
    RefPtr<Font> font(const FontDescription&, bool syntheticBold, bool syntheticItalic, const FontCreationContext&);

    FontLoadRequest* fontLoadRequest() const { return m_fontRequest.get(); }
    bool requiresExternalResource() const { return m_fontRequest.get(); }

    bool isSVGFontFaceSource() const;

private:
    bool shouldIgnoreFontLoadCompletions() const;

    void fontLoaded(FontLoadRequest&) override;

    void setStatus(Status);

    AtomString m_familyNameOrURI; // URI for remote, built-in font name for local.
    CSSFontFace& m_face; // Our owning font face.
    WeakPtr<CSSFontSelector> m_fontSelector; // For remote fonts, to orchestrate loading.
    std::unique_ptr<FontLoadRequest> m_fontRequest; // Also for remote fonts, a pointer to the resource request.

    RefPtr<SharedBuffer> m_generatedOTFBuffer;
    RefPtr<JSC::ArrayBufferView> m_immediateSource;
    std::unique_ptr<FontCustomPlatformData> m_immediateFontCustomPlatformData;

    WeakPtr<SVGFontFaceElement> m_svgFontFaceElement;
    std::unique_ptr<FontCustomPlatformData> m_inDocumentCustomPlatformData;

    Status m_status { Status::Pending };
    bool m_hasSVGFontFaceElement { false };
};

} // namespace WebCore
