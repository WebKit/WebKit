/*
 * Copyright (C) 2007-2022 Apple Inc. All rights reserved.
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
#include "RenderStyleConstants.h"
#include "Settings.h"
#include "TextFlags.h"
#include <memory>
#include <wtf/Forward.h>
#include <wtf/HashSet.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class CSSFontFaceSource;
class CSSFontSelector;
class CSSPrimitiveValue;
class CSSValue;
class CSSValueList;
class Document;
class Font;
class FontDescription;
class FontFace;
class FontFeatureValues;
class FontPaletteValues;
class MutableStyleProperties;
class ScriptExecutionContext;
class StyleProperties;
class StyleRuleFontFace;

enum class ExternalResourceDownloadPolicy;

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(CSSFontFace);
class CSSFontFace final : public RefCounted<CSSFontFace> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(CSSFontFace);
public:
    static Ref<CSSFontFace> create(CSSFontSelector&, StyleRuleFontFace* cssConnection = nullptr, FontFace* wrapper = nullptr, bool isLocalFallback = false);
    virtual ~CSSFontFace();

    void setFamilies(CSSValueList&);
    void setStyle(CSSValue&);
    void setWeight(CSSValue&);
    void setStretch(CSSValue&);
    void setUnicodeRange(CSSValueList&);
    void setFeatureSettings(CSSValue&);
    void setDisplay(CSSPrimitiveValue&);

    String family() const;
    String style() const;
    String weight() const;
    String stretch() const;
    String unicodeRange() const;
    String featureSettings() const;
    String display() const;

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

    RefPtr<CSSValueList> families() const;
    Span<const UnicodeRange> ranges() const { ASSERT(m_status != Status::Failure); return m_ranges.span(); }

    void setFontSelectionCapabilities(FontSelectionCapabilities capabilities) { m_fontSelectionCapabilities = capabilities; }
    FontSelectionCapabilities fontSelectionCapabilities() const { ASSERT(m_status != Status::Failure); return m_fontSelectionCapabilities.computeFontSelectionCapabilities(); }

    bool isLocalFallback() const { return m_isLocalFallback; }
    Status status() const { return m_status; }
    StyleRuleFontFace* cssConnection() const;

    class Client;
    void addClient(Client&);
    void removeClient(Client&);

    bool computeFailureState() const;

    void opportunisticallyStartFontDataURLLoading();

    void adoptSource(std::unique_ptr<CSSFontFaceSource>&&);
    void sourcesPopulated() { m_sourcesPopulated = true; }
    size_t sourceCount() const { return m_sources.size(); }

    void fontLoaded(CSSFontFaceSource&);

    void load();

    RefPtr<Font> font(const FontDescription&, bool syntheticBold, bool syntheticItalic, ExternalResourceDownloadPolicy, const FontPaletteValues&, RefPtr<FontFeatureValues>);

    static void appendSources(CSSFontFace&, CSSValueList&, ScriptExecutionContext*, bool isInitiatingElementInUserAgentShadowTree);

    class Client {
    public:
        virtual ~Client() = default;
        virtual void fontLoaded(CSSFontFace&) { }
        virtual void fontStateChanged(CSSFontFace&, Status /*oldState*/, Status /*newState*/) { }
        virtual void fontPropertyChanged(CSSFontFace&, CSSValueList* /*oldFamilies*/ = nullptr) { }
        virtual void updateStyleIfNeeded(CSSFontFace&) { }
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
    Ref<FontFace> wrapper(ScriptExecutionContext*);
    void setWrapper(FontFace&);
    FontFace* existingWrapper();

    struct FontLoadTiming {
        Seconds blockPeriod;
        Seconds swapPeriod;
    };
    FontLoadTiming fontLoadTiming() const;
    bool shouldIgnoreFontLoadCompletions() const { return m_shouldIgnoreFontLoadCompletions; }

    bool purgeable() const;

    AllowUserInstalledFonts allowUserInstalledFonts() const { return m_allowUserInstalledFonts; }

    void updateStyleIfNeeded();

    bool hasSVGFontFaceSource() const;
    void setErrorState();

private:
    CSSFontFace(const Settings::Values*, StyleRuleFontFace*, FontFace*, bool isLocalFallback);

    size_t pump(ExternalResourceDownloadPolicy);
    void setStatus(Status);
    void notifyClientsOfFontPropertyChange();

    void initializeWrapper();

    void fontLoadEventOccurred();
    void timeoutFired();

    const StyleProperties& properties() const;
    MutableStyleProperties& mutableProperties();

    Document* document();

    const std::variant<Ref<MutableStyleProperties>, Ref<StyleRuleFontFace>> m_propertiesOrCSSConnection;
    RefPtr<CSSValueList> m_families;
    Vector<UnicodeRange> m_ranges;

    FontFeatureSettings m_featureSettings;
    FontLoadingBehavior m_loadingBehavior { FontLoadingBehavior::Auto };

    Vector<std::unique_ptr<CSSFontFaceSource>, 0, CrashOnOverflow, 0> m_sources;
    HashSet<Client*> m_clients;
    WeakPtr<FontFace> m_wrapper;
    FontSelectionSpecifiedCapabilities m_fontSelectionCapabilities;
    
    Status m_status { Status::Pending };
    bool m_isLocalFallback { false };
    bool m_sourcesPopulated { false };
    bool m_mayBePurged { true };
    bool m_shouldIgnoreFontLoadCompletions { false };
    FontLoadTimingOverride m_fontLoadTimingOverride { FontLoadTimingOverride::None };
    AllowUserInstalledFonts m_allowUserInstalledFonts { AllowUserInstalledFonts::Yes };

    Timer m_timeoutTimer;
};

}
