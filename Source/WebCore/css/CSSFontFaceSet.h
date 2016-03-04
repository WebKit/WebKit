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

#ifndef CSSFontFaceSet_h
#define CSSFontFaceSet_h

#include "CSSFontFace.h"
#include <wtf/HashMap.h>
#include <wtf/Vector.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

class CSSPrimitiveValue;
class FontFaceSet;

class CSSFontFaceSetClient {
public:
    virtual ~CSSFontFaceSetClient() { }
    virtual void faceFinished(CSSFontFace&, CSSFontFace::Status) { };
    virtual void fontModified() { };
    virtual void startedLoading() { };
    virtual void completedLoading() { };
};

class CSSFontFaceSet final : public RefCounted<CSSFontFaceSet>, public CSSFontFace::Client {
public:
    static Ref<CSSFontFaceSet> create()
    {
        return adoptRef(*new CSSFontFaceSet());
    }
    ~CSSFontFaceSet();

    void addClient(CSSFontFaceSetClient&);
    void removeClient(CSSFontFaceSetClient&);

    bool hasFace(const CSSFontFace&) const;
    size_t faceCount() const { return m_faces.size(); }
    void add(CSSFontFace&);
    void remove(const CSSFontFace&);
    void clear();
    CSSFontFace& operator[](size_t i);

    bool check(const String& font, const String& text, ExceptionCode&);

    CSSSegmentedFontFace* getFontFace(FontTraitsMask, const AtomicString& family);

    enum class Status {
        Loading,
        Loaded
    };
    Status status() const { return m_status; }

    Vector<std::reference_wrapper<CSSFontFace>> matchingFaces(const String& font, const String& text, ExceptionCode&);

private:
    CSSFontFaceSet();

    void removeFromFacesLookupTable(const CSSFontFace&, const CSSValueList& familiesToSearchFor);
    void addToFacesLookupTable(CSSFontFace&);

    void incrementActiveCount();
    void decrementActiveCount();

    void fontStateChanged(CSSFontFace&, CSSFontFace::Status oldState, CSSFontFace::Status newState) override;
    void fontPropertyChanged(CSSFontFace&, CSSValueList* oldFamilies = nullptr) override;

    void registerLocalFontFacesForFamily(const String&);

    static String familyNameFromPrimitive(const CSSPrimitiveValue&);

    // m_faces should hold all the same fonts as the ones inside inside m_facesLookupTable.
    Vector<Ref<CSSFontFace>> m_faces; // We should investigate moving m_faces to FontFaceSet and making it reference FontFaces. This may clean up the font loading design.
    HashMap<String, Vector<Ref<CSSFontFace>>, ASCIICaseInsensitiveHash> m_facesLookupTable;
    HashMap<String, Vector<Ref<CSSFontFace>>, ASCIICaseInsensitiveHash> m_locallyInstalledFacesLookupTable;
    HashMap<String, HashMap<unsigned, std::unique_ptr<CSSSegmentedFontFace>>, ASCIICaseInsensitiveHash> m_cache;
    size_t m_facesPartitionIndex { 0 }; // All entries in m_faces before this index are CSS-connected.
    Status m_status { Status::Loaded };
    HashSet<CSSFontFaceSetClient*> m_clients;
    unsigned m_activeCount { 0 };
};

}

#endif
