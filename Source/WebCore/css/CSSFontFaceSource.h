/*
 * Copyright (C) 2007, 2008, 2011 Apple Inc. All rights reserved.
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

#include "CachedFontClient.h"
#include "CachedResourceHandle.h"
#include <JavaScriptCore/ArrayBufferView.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/AtomString.h>

namespace WebCore {

class CSSFontFace;
class CSSFontSelector;
class Font;
struct FontCustomPlatformData;
class FontDescription;
struct FontSelectionSpecifiedCapabilities;
struct FontVariantSettings;
class SVGFontFaceElement;
class SharedBuffer;

template <typename T> class FontTaggedSettings;
typedef FontTaggedSettings<int> FontFeatureSettings;

class CSSFontFaceSource final : public CachedFontClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    CSSFontFaceSource(CSSFontFace& owner, const String& familyNameOrURI, CachedFont* = nullptr, SVGFontFaceElement* = nullptr, RefPtr<JSC::ArrayBufferView>&& = nullptr);
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

    void opportunisticallyStartFontDataURLLoading(CSSFontSelector&);

    void load(CSSFontSelector*);
    RefPtr<Font> font(const FontDescription&, bool syntheticBold, bool syntheticItalic, const FontFeatureSettings&, FontSelectionSpecifiedCapabilities);

    CachedFont* cachedFont() const { return m_font.get(); }
    bool requiresExternalResource() const { return m_font; }

    bool isSVGFontFaceSource() const;

private:
    bool shouldIgnoreFontLoadCompletions() const;

    void fontLoaded(CachedFont&) override;

    void setStatus(Status);

    AtomString m_familyNameOrURI; // URI for remote, built-in font name for local.
    CachedResourceHandle<CachedFont> m_font; // For remote fonts, a pointer to our cached resource.
    CSSFontFace& m_face; // Our owning font face.

    RefPtr<SharedBuffer> m_generatedOTFBuffer;
    RefPtr<JSC::ArrayBufferView> m_immediateSource;
    std::unique_ptr<FontCustomPlatformData> m_immediateFontCustomPlatformData;

    WeakPtr<SVGFontFaceElement> m_svgFontFaceElement;
    std::unique_ptr<FontCustomPlatformData> m_inDocumentCustomPlatformData;

    Status m_status { Status::Pending };
    bool m_hasSVGFontFaceElement;
};

} // namespace WebCore
