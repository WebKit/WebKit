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
#include <wtf/Vector.h>

namespace WebCore {

class FontFaceSet;

class CSSFontFaceSetClient {
public:
    virtual ~CSSFontFaceSetClient() { }
    virtual void faceFinished(CSSFontFace&, CSSFontFace::Status) = 0;
    virtual void startedLoading() = 0;
    virtual void completedLoading() = 0;
};

class CSSFontFaceSet final : public CSSFontFace::Client {
public:
    CSSFontFaceSet(CSSFontFaceSetClient&);
    ~CSSFontFaceSet();

    bool hasFace(const CSSFontFace&) const;
    size_t faceCount() const { return m_faces.size(); }
    void add(CSSFontFace&);
    void remove(const CSSFontFace&);
    const CSSFontFace& operator[](size_t i) const { return m_faces[i]; }

    void load(const String& font, const String& text, ExceptionCode&);
    bool check(const String& font, const String& text, ExceptionCode&);

    enum class Status {
        Loading,
        Loaded
    };
    Status status() const { return m_status; }

    Vector<std::reference_wrapper<CSSFontFace>> matchingFaces(const String& font, const String& text, ExceptionCode&);

private:
    void incrementActiveCount();
    void decrementActiveCount();

    virtual void stateChanged(CSSFontFace&, CSSFontFace::Status oldState, CSSFontFace::Status newState) override;

    Vector<Ref<CSSFontFace>> m_faces;
    Status m_status { Status::Loaded };
    CSSFontFaceSetClient& m_client;
    unsigned m_activeCount { 0 };
};

}

#endif
