/*
 * CSS Media Query
 *
 * Copyright (C) 2005, 2006 Kimmo Kinnunen <kimmo.t.kinnunen@nokia.com>.
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "MediaQuery.h"

#include "MediaQueryExp.h"

namespace WebCore {

MediaQuery::MediaQuery(Restrictor r, const String& mediaType, Vector<MediaQueryExp*>* exprs)
    : m_restrictor(r)
    , m_mediaType(mediaType)
    , m_expressions(exprs)
{
    if (!m_expressions)
        m_expressions = new Vector<MediaQueryExp*>;
}

MediaQuery::~MediaQuery()
{
    if (m_expressions) {
        deleteAllValues(*m_expressions);
        delete m_expressions;
    }
}

bool MediaQuery::operator==(const MediaQuery& other) const
{
    if (m_restrictor != other.m_restrictor
        || m_mediaType != other.m_mediaType
        || m_expressions->size() != other.m_expressions->size())
        return false;

    for (size_t i = 0; i < m_expressions->size(); ++i) {
        MediaQueryExp* exp = m_expressions->at(i);
        MediaQueryExp* oexp = other.m_expressions->at(i);
        if (!(*exp == *oexp))
            return false;
    }

    return true;
}

String MediaQuery::cssText() const
{
    String text;
    switch (m_restrictor) {
        case MediaQuery::Only:
            text += "only ";
            break;
        case MediaQuery::Not:
            text += "not ";
            break;
        case MediaQuery::None:
        default:
            break;
    }
    text += m_mediaType;
    for (size_t i = 0; i < m_expressions->size(); ++i) {
        MediaQueryExp* exp = m_expressions->at(i);
        text += " and (";
        text += exp->mediaFeature();
        if (exp->value()) {
            text += ": ";
            text += exp->value()->cssText();
        }
        text += ")";
    }
    return text;
}

} //namespace
