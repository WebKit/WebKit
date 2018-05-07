/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "config.h"
#include "DisplayBox.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include <wtf/IsoMallocInlines.h>

namespace WebCore {
namespace Display {

WTF_MAKE_ISO_ALLOCATED_IMPL(Box);

Box::Box()
{
}

Box::~Box()
{
}

LayoutRect Box::marginBox() const
{
    ASSERT(m_hasValidMargin);
    auto marginBox = rect();
    auto topLeftMargin = LayoutSize(m_marginLeft, m_marginTop);
    marginBox.inflate(topLeftMargin);

    auto bottomRightMargin = LayoutSize(m_marginRight, m_marginBottom);
    marginBox.expand(bottomRightMargin);
    return marginBox;
}

LayoutRect Box::borderBox() const
{
    return LayoutRect(LayoutPoint(0, 0), size());
}

LayoutRect Box::paddingBox() const
{
    ASSERT(m_hasValidBorder);
    auto paddingBox = borderBox();
    auto topLeftBorder = LayoutSize(m_borderLeft, m_borderTop);
    paddingBox.inflate(-topLeftBorder);

    auto bottomRightBorder = LayoutSize(m_borderRight, m_borderBottom);
    paddingBox.expand(-bottomRightBorder);
    return paddingBox;
}

LayoutRect Box::contentBox() const
{
    ASSERT(m_hasValidPadding);
    auto contentBox = paddingBox();
    auto topLeftPadding = LayoutSize(m_paddingLeft, m_paddingTop);
    contentBox.inflate(-topLeftPadding);
    
    auto bottomRightPadding = LayoutSize(m_paddingRight, m_paddingBottom);
    contentBox.expand(-bottomRightPadding);
    return contentBox;
}

}
}

#endif
