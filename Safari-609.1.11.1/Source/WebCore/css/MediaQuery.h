/*
 * CSS Media Query
 *
 * Copyright (C) 2006 Kimmo Kinnunen <kimmo.t.kinnunen@nokia.com>.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY
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

#include "MediaQueryExpression.h"
#include <wtf/Vector.h>

namespace WebCore {

class MediaQuery {
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum Restrictor { Only, Not, None };

    MediaQuery(Restrictor, const String& mediaType, Vector<MediaQueryExpression>&&);

    Restrictor restrictor() const { return m_restrictor; }
    const Vector<MediaQueryExpression>& expressions() const { return m_expressions; }
    const String& mediaType() const { return m_mediaType; }
    bool ignored() const { return m_ignored; }

    const String& cssText() const;

    bool operator==(const MediaQuery& other) const;

    void shrinkToFit() { m_expressions.shrinkToFit(); }

private:
    String serialize() const;

    String m_mediaType;
    mutable String m_serializationCache;
    Vector<MediaQueryExpression> m_expressions;
    Restrictor m_restrictor;
    bool m_ignored { false };
};

WTF::TextStream& operator<<(WTF::TextStream&, const MediaQuery&);

} // namespace
