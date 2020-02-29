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

#include "FontSelectionValueInlines.h"
#include "FontTaggedSettings.h"
#include "StyleRule.h"
#include "TextFlags.h"
#include "Timer.h"
#include <memory>
#include <wtf/Forward.h>
#include <wtf/HashSet.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

namespace JSC {
class CallFrame;
}

namespace WebCore {

class CSSFontFaceSource;
class CSSFontSelector;
class CSSSegmentedFontFace;
class CSSValue;
class CSSValueList;
class Document;
class FontDescription;
class Font;
class FontFace;
enum class ExternalResourceDownloadPolicy;

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(CSSFontFace);
class CSSFontFace final : public RefCounted<CSSFontFace> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(CSSFontFace);
public:
    static Ref<CSSFontFace> create(CSSFontSelector* fontSelector, StyleRuleFontFace* cssConnection = nullptr, FontFace* wrapper = nullptr, bool isLocalFallback = false)
    {
        return adoptRef(*new CSSFontFace(fontSelector, cssConnection, wrapper, isLocalFallback));
    }
    virtual ~CSSFontFace();

    // FIXME: These functions don't need to have boolean return values.
    // Callers only call this with known-valid CSS values.
    bool setFamilies(CSSValue&);
    void setStyle(CSSValue&);
    void setWeight(CSSValue&);
    void setStretch(CSSValue&);
    bool setUnicodeRange(CSSValue&);
    void setFeatureSettings(CSSValue&);
    void setLoadingBehavior(CSSValue&);

    // Pending => Loading  => TimedOut
    //              ||  \\    //  ||
    //              ||   \\  //   ||
    //              ||    \\//    ||
    //              ||     //     ||
    //              ||    //\\    ||
    //              ||   //  \\   ||
    //              \/  \/    \/  \/
    //             Success    Failure
    enum class Status : uint8_t { Pending, Loading, TimedOut, Success, Failure };
    
    struct UnicodeRange;
    
    // Optional return values to represent default string for members of FontFace.h
    const Optional<CSSValueList*> families() const { return m_status == Status::Failure ? WTF::nullopt : static_cast<Optional<CSSValueList*>>(m_families.get()); }
    Optional<FontSelectionRange> weight() const { return m_status == Status::Failure ? WTF::nullopt : static_cast<Optional<FontSelectionRange>>(m_fontSelectionCapabilities.computeWeight()); }
    Optional<FontSelectionRange> stretch() const { return m_status == Status::Failure ? WTF::nullopt : static_cast<Optional<FontSelectionRange>>(m_fontSelectionCapabilities.computeWidth()); }
    Optional<FontSelectionRange> italic() const { return m_status == Status::Failure ? WTF::nullopt : static_cast<Optional<FontSelectionRange>>(m_fontSelectionCapabilities.computeSlope()); }
    Optional<FontSelectionCapabilities> fontSelectionCapabilities() const { return m_status == Status::Failure ? WTF::nullopt : static_cast<Optional<FontSelectionCapabilities>>(m_fontSelectionCapabilities.computeFontSelectionCapabilities()); }
    const Optional<Vector<UnicodeRange>> ranges() const { return m_status == Status::Failure ? WTF::nullopt : static_cast<Optional<Vector<UnicodeRange>>>(m_ranges); }
    const Optional<FontFeatureSettings> featureSettings() const { return m_status == Status::Failure ? WTF::nullopt : static_cast<Optional<FontFeatureSettings>>(m_featureSettings); }
    Optional<FontLoadingBehavior> loadingBehavior() const { return m_status == Status::Failure ? WTF::nullopt :  static_cast<Optional<FontLoadingBehavior>>(m_loadingBehavior); }
    void setWeight(FontSelectionRange weight) { m_fontSelectionCapabilities.weight = weight; }
    void setStretch(FontSelectionRange stretch) { m_fontSelectionCapabilities.width = stretch; }
    void setStyle(FontSelectionRange italic) { m_fontSelectionCapabilities.slope = italic; }
    void setFontSelectionCapabilities(FontSelectionCapabilities capabilities) { m_fontSelectionCapabilities = capabilities; }
    bool isLocalFallback() const { return m_isLocalFallback; }
    Status status() const { return m_status; }
    StyleRuleFontFace* cssConnection() const { return m_cssConnection.get(); }

    class Client;
    void addClient(Client&);
    void removeClient(Client&);

    bool computeFailureState() const;

    void opportunisticallyStartFontDataURLLoading(CSSFontSelector&);

    void adoptSource(std::unique_ptr<CSSFontFaceSource>&&);
    void sourcesPopulated() { m_sourcesPopulated = true; }

    void fontLoaded(CSSFontFaceSource&);

    void load();

    RefPtr<Font> font(const FontDescription&, bool syntheticBold, bool syntheticItalic, ExternalResourceDownloadPolicy);

    static void appendSources(CSSFontFace&, CSSValueList&, Document*, bool isInitiatingElementInUserAgentShadowTree);

    class Client {
    public:
        virtual ~Client() = default;
        virtual void fontLoaded(CSSFontFace&) { }
        virtual void fontStateChanged(CSSFontFace&, Status /*oldState*/, Status /*newState*/) { }
        virtual void fontPropertyChanged(CSSFontFace&, CSSValueList* /*oldFamilies*/ = nullptr) { }
        virtual void ref() = 0;
        virtual void deref() = 0;
    };

    struct UnicodeRange {
        UChar32 from;
        UChar32 to;
        bool operator==(const UnicodeRange& other) const { return from == other.from && to == other.to; }
        bool operator!=(const UnicodeRange& other) const { return !(*this == other); }
    };

    bool rangesMatchCodePoint(UChar32) const;

    // We don't guarantee that the FontFace wrapper will be the same every time you ask for it.
    Ref<FontFace> wrapper();
    void setWrapper(FontFace&);
    FontFace* existingWrapper();

    struct FontLoadTiming {
        Seconds blockPeriod;
        Seconds swapPeriod;
    };
    FontLoadTiming fontLoadTiming() const;
    bool shouldIgnoreFontLoadCompletions() const;

    bool purgeable() const;

    AllowUserInstalledFonts allowUserInstalledFonts() const;

    void updateStyleIfNeeded();

#if ENABLE(SVG_FONTS)
    bool hasSVGFontFaceSource() const;
#endif
    void setErrorState();

    Document* document() const;

private:
    CSSFontFace(CSSFontSelector*, StyleRuleFontFace*, FontFace*, bool isLocalFallback);

    size_t pump(ExternalResourceDownloadPolicy);
    void setStatus(Status);
    void notifyClientsOfFontPropertyChange();

    void initializeWrapper();

    void fontLoadEventOccurred();
    void timeoutFired();

    RefPtr<CSSValueList> m_families;
    Vector<UnicodeRange> m_ranges;

    FontFeatureSettings m_featureSettings;
    FontLoadingBehavior m_loadingBehavior { FontLoadingBehavior::Auto };

    Vector<std::unique_ptr<CSSFontFaceSource>, 0, CrashOnOverflow, 0> m_sources;
    WeakPtr<CSSFontSelector> m_fontSelector; // FIXME: Ideally this data member would go away (https://bugs.webkit.org/show_bug.cgi?id=208351).
    RefPtr<StyleRuleFontFace> m_cssConnection;
    HashSet<Client*> m_clients;
    WeakPtr<FontFace> m_wrapper;
    FontSelectionSpecifiedCapabilities m_fontSelectionCapabilities;
    
    Status m_status { Status::Pending };
    bool m_isLocalFallback { false };
    bool m_sourcesPopulated { false };
    bool m_mayBePurged { true };

    Timer m_timeoutTimer;
};

}
