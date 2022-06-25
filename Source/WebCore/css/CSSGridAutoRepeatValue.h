/*
 * Copyright (C) 2016 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CSSValueKeywords.h"
#include "CSSValueList.h"

namespace WebCore {

// CSSGridAutoRepeatValue stores the track sizes and line numbers when the auto-repeat
// syntax is used
//
// Right now the auto-repeat syntax is as follows:
// <auto-repeat>  = repeat( [ auto-fill | auto-fit ], <line-names>? <fixed-size> <line-names>? )
//
// meaning that only one fixed size track is allowed. It could be argued that a different
// class storing two CSSGridLineNamesValue and one CSSValue (for the track size) fits
// better but the CSSWG has left the door open to allow more than one track in the
// future. That's why we're using a list, it's prepared for future changes and it also
// allows us to keep the parsing algorithm almost intact.
class CSSGridAutoRepeatValue final : public CSSValueList {
public:
    static Ref<CSSGridAutoRepeatValue> create(CSSValueID id)
    {
        return adoptRef(*new CSSGridAutoRepeatValue(id));
    }

    String customCSSText() const;
    bool equals(const CSSGridAutoRepeatValue&) const;

    CSSValueID autoRepeatID() const { return m_autoRepeatID; }

private:
    CSSGridAutoRepeatValue(CSSValueID id)
        : CSSValueList(GridAutoRepeatClass, SpaceSeparator)
        , m_autoRepeatID(id)
    {
        ASSERT(id == CSSValueAutoFill || id == CSSValueAutoFit);
    }

    const CSSValueID m_autoRepeatID;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSGridAutoRepeatValue, isGridAutoRepeatValue());
