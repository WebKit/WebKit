/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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
#include <wtf/HashMap.h>
#include <wtf/Vector.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

class CSSPrimitiveValue;
class FontFaceSet;

class CSSFontFaceSetClient {
public:
    virtual ~CSSFontFaceSetClient() = default;
    virtual void faceFinished(CSSFontFace&, CSSFontFace::Status) { };
    virtual void fontModified() { };
    virtual void startedLoading() { };
    virtual void completedLoading() { };
};

class CSSFontFaceSet final : public RefCounted<CSSFontFaceSet>, public CSSFontFace::Client {
public:
    static Ref<CSSFontFaceSet> create(CSSFontSelector* owningFontSelector = nullptr)
    {
        return adoptRef(*new CSSFontFaceSet(owningFontSelector));
    }
    ~CSSFontFaceSet();

    void addClient(CSSFontFaceSetClient&);
    void removeClient(CSSFontFaceSetClient&);

    bool hasFace(const CSSFontFace&) const;
    size_t faceCount() const { return m_faces.size(); }
    void add(CSSFontFace&);
    void remove(const CSSFontFace&);
    void purge();
    void emptyCaches();
    void clear();
    CSSFontFace& operator[](size_t i);

    CSSFontFace* lookUpByCSSConnection(StyleRuleFontFace&);

    ExceptionOr<bool> check(const String& font, const String& text);

    CSSSegmentedFontFace* fontFace(FontSelectionRequest, const AtomicString& family);

    enum class Status { Loading, Loaded };
    Status status() const { return m_status; }

    bool hasActiveFontFaces() { return status() == Status::Loading; }

    ExceptionOr<Vector<std::reference_wrapper<CSSFontFace>>> matchingFacesExcludingPreinstalledFonts(const String& font, const String& text);

    // CSSFontFace::Client needs to be able to be held in a RefPtr.
    void ref() final { RefCounted::ref(); }
    void deref() final { RefCounted::deref(); }

private:
    CSSFontFaceSet(CSSFontSelector*);

    void removeFromFacesLookupTable(const CSSFontFace&, const CSSValueList& familiesToSearchFor);
    void addToFacesLookupTable(CSSFontFace&);

    void incrementActiveCount();
    void decrementActiveCount();

    void fontStateChanged(CSSFontFace&, CSSFontFace::Status oldState, CSSFontFace::Status newState) final;
    void fontPropertyChanged(CSSFontFace&, CSSValueList* oldFamilies = nullptr) final;

    void ensureLocalFontFacesForFamilyRegistered(const String&);

    static String familyNameFromPrimitive(const CSSPrimitiveValue&);

    using FontSelectionKey = Optional<FontSelectionRequest>;
    struct FontSelectionKeyHash {
        static unsigned hash(const FontSelectionKey& key) { return computeHash(key); }
        static bool equal(const FontSelectionKey& a, const FontSelectionKey& b) { return a == b; }
        static const bool safeToCompareToEmptyOrDeleted = true;
    };
    struct FontSelectionKeyHashTraits : SimpleClassHashTraits<FontSelectionKey> {
        static const bool emptyValueIsZero = false;
        static FontSelectionKey emptyValue() { return FontSelectionRequest { }; }
        static void constructDeletedValue(FontSelectionKey& slot) { slot = WTF::nullopt; }
        static bool isDeletedValue(const FontSelectionKey& value) { return !value; }
    };
    using FontSelectionHashMap = HashMap<FontSelectionKey, RefPtr<CSSSegmentedFontFace>, FontSelectionKeyHash, FontSelectionKeyHashTraits>;

    // m_faces should hold all the same fonts as the ones inside inside m_facesLookupTable.
    Vector<Ref<CSSFontFace>> m_faces; // We should investigate moving m_faces to FontFaceSet and making it reference FontFaces. This may clean up the font loading design.
    HashMap<String, Vector<Ref<CSSFontFace>>, ASCIICaseInsensitiveHash> m_facesLookupTable;
    HashMap<String, Vector<Ref<CSSFontFace>>, ASCIICaseInsensitiveHash> m_locallyInstalledFacesLookupTable;
    HashMap<String, FontSelectionHashMap, ASCIICaseInsensitiveHash> m_cache;
    HashMap<StyleRuleFontFace*, CSSFontFace*> m_constituentCSSConnections;
    size_t m_facesPartitionIndex { 0 }; // All entries in m_faces before this index are CSS-connected.
    Status m_status { Status::Loaded };
    HashSet<CSSFontFaceSetClient*> m_clients;
    CSSFontSelector* m_owningFontSelector;
    unsigned m_activeCount { 0 };
};

}
