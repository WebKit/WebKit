/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(SPATIAL_PORTAL)

#include "CSSCustomIdent.h"
#include "CSSValue.h"

namespace WebCore {

// <dashed-ident> '#' <ident>, as accepted by position-anchor: an anchor name paired with the name
// of an attachment point inside the anchor's model asset. The '#' is a delimiter within a single
// anchor reference rather than a separator between two independent values, so this is deliberately
// not a CSSValuePair.
class CSSPinnedAnchorNameValue final : public CSSValue {
public:
    static Ref<CSSPinnedAnchorNameValue> create(CSS::CustomIdent name, CSS::CustomIdent attachment);

    const CSS::CustomIdent& name() const LIFETIME_BOUND { return m_name; }
    const CSS::CustomIdent& attachment() const LIFETIME_BOUND { return m_attachment; }

    String customCSSText(const CSS::SerializationContext&) const;
    bool equals(const CSSPinnedAnchorNameValue&) const;
    bool NODELETE addDerivedHash(Hasher&) const;

private:
    CSSPinnedAnchorNameValue(CSS::CustomIdent&&, CSS::CustomIdent&&);

    CSS::CustomIdent m_name;
    CSS::CustomIdent m_attachment;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSPinnedAnchorNameValue, isPinnedAnchorNameValue())

#endif // ENABLE(SPATIAL_PORTAL)
