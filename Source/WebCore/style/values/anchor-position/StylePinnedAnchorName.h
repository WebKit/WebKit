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

#include <WebCore/StyleAnchorName.h>
#include <WebCore/StyleCustomIdent.h>
#include <WebCore/StyleValueTypes.h>

namespace WebCore {
namespace Style {

// <pinned-anchor-name> = <anchor-name> [ '#' <ident> ]?
//
// An <anchor-name> plus the name of an attachment point inside that anchor's model asset. The anchor
// name is tree-scoped; the attachment point comes from the asset file, so it isn't.
//
// Proposed in the Spatial CSS explainer, not yet in a specification.
// https://github.com/WebKit/explainers/blob/main/css-spatial/explainer.md
struct PinnedAnchorName {
    AnchorName name;
    CustomIdent attachment;

    bool operator==(const PinnedAnchorName&) const = default;
};

// MARK: - Conversion

template<> struct CSSValueCreation<PinnedAnchorName> {
    Ref<CSSValue> operator()(CSSValuePool&, const Style::ComputedStyle&, const PinnedAnchorName&);
};

// MARK: - Serialization

template<> struct Serialize<PinnedAnchorName> {
    void operator()(StringBuilder&, const CSS::SerializationContext&, const Style::ComputedStyle&, const PinnedAnchorName&);
};

// MARK: - Logging

WTF::TextStream& operator<<(WTF::TextStream&, const PinnedAnchorName&);

} // namespace Style
} // namespace WebCore

#endif // ENABLE(SPATIAL_PORTAL)
