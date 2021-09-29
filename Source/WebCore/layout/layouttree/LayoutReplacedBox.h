/*
 * Copyright (C) 2018-2020 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "CachedImage.h"
#include "LayoutBox.h"
#include "LayoutSize.h"
#include "LayoutUnit.h"
#include <wtf/IsoMalloc.h>

namespace WebCore {
namespace Layout {

class ReplacedBox : public Box {
    WTF_MAKE_ISO_ALLOCATED(ReplacedBox);
public:
    ReplacedBox(std::optional<ElementAttributes>, RenderStyle&&, std::unique_ptr<RenderStyle>&& firstLineStyle = nullptr);
    virtual ~ReplacedBox() = default;

    void setCachedImage(CachedImage& cachedImage) { m_cachedImage = &cachedImage; }
    CachedImage* cachedImage() const { return m_cachedImage; }

    // FIXME: Temporary until after intrinsic size change is tracked internally.
    void setIntrinsicSize(LayoutSize size) { m_intrinsicSize = size; }
    void setIntrinsicRatio(LayoutUnit ratio) { m_intrinsicRatio = ratio; };

    bool hasIntrinsicWidth() const;
    bool hasIntrinsicHeight() const;
    bool hasIntrinsicRatio() const;
    LayoutUnit intrinsicWidth() const;
    LayoutUnit intrinsicHeight() const;
    LayoutUnit intrinsicRatio() const;

    void setBaseline(LayoutUnit baseline) { m_baseline = baseline; }
    std::optional<LayoutUnit> baseline() const { return m_baseline; }

private:
    bool hasAspectRatio() const;

    std::optional<LayoutSize> m_intrinsicSize;
    std::optional<LayoutUnit> m_intrinsicRatio;
    std::optional<LayoutUnit> m_baseline;
    CachedImage* m_cachedImage { nullptr };
};

}
}

SPECIALIZE_TYPE_TRAITS_LAYOUT_BOX(ReplacedBox, isReplacedBox())

#endif
