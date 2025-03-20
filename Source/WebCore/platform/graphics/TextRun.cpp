/*
 * Copyright (C) 2011, 2014, 2022 Apple Inc. All rights reserved.
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
#include "TextRun.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(TextRun);

struct ExpectedTextRunSize final : public CanMakeCheckedPtr<ExpectedTextRunSize> {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    WTF_STRUCT_OVERRIDE_DELETE_FOR_CHECKED_PTR(ExpectedTextRunSize);

    String text;
    TabSize tabSize;
    float float1;
    float float2;
    float float3;
    ExpansionBehavior expansionBehavior;
    TextSpacing::SpacingState spacingState;
    unsigned bitfields : 5;
};

static_assert(sizeof(TextRun) == sizeof(ExpectedTextRunSize), "TextRun should be small");

TextStream& operator<<(TextStream& ts, const TextRun& textRun)
{
    ts.dumpProperty("text"_s, textRun.text());
    ts.dumpProperty("tab-size"_s, textRun.tabSize());
    ts.dumpProperty("x-pos"_s, textRun.xPos());
    ts.dumpProperty("horizontal-glyph-streatch"_s, textRun.horizontalGlyphStretch());
    ts.dumpProperty("expansion"_s, textRun.expansion());
    ts.dumpProperty("expansion-behavior"_s, textRun.expansionBehavior());
    ts.dumpProperty("allow-tabs"_s, textRun.allowTabs());
    ts.dumpProperty("direction"_s, textRun.direction());
    ts.dumpProperty("directional-override"_s, textRun.directionalOverride());
    ts.dumpProperty("character-scan-for-code-path"_s, textRun.characterScanForCodePath());
    ts.dumpProperty("spacing-disabled"_s, textRun.spacingDisabled());
    return ts;
}

}
