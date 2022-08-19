/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "InlineIteratorBoxLegacyPath.h"
#include "InlineIteratorBoxModernPath.h"
#include "LayoutIntegrationInlineContent.h"

namespace WebCore {

struct PaintInfo;
class LegacyEllipsisBox;

template<typename EllipsisBoxPath>
class EllipsisBoxPainter {
public:
    EllipsisBoxPainter(EllipsisBoxPath&&, PaintInfo&, const LayoutPoint& paintOffset);

    void paint();

private:
    EllipsisBoxPath m_ellipsisBox;
    PaintInfo& m_paintInfo;
    LayoutPoint m_paintOffset;
};

class LegacyEllipsisBoxPainter : public EllipsisBoxPainter<InlineIterator::BoxLegacyPath> {
public:
    LegacyEllipsisBoxPainter(const LegacyEllipsisBox&, PaintInfo&, const LayoutPoint& paintOffset);
};

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
class ModernEllipsisBoxPainter : public EllipsisBoxPainter<InlineIterator::BoxModernPath> {
public:
    ModernEllipsisBoxPainter(const LayoutIntegration::InlineContent&, const InlineDisplay::Box&, PaintInfo&, const LayoutPoint& paintOffset);
};
#endif

}
