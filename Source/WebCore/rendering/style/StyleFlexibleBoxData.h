/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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
 *  THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 *  EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 *  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#pragma once

#include <WebCore/RenderStyleConstants.h>
#include <WebCore/StyleFlexBasis.h>
#include <WebCore/StyleFlexGrow.h>
#include <WebCore/StyleFlexShrink.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>

namespace WebCore {

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StyleFlexibleBoxData);
class StyleFlexibleBoxData : public RefCounted<StyleFlexibleBoxData> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(StyleFlexibleBoxData, StyleFlexibleBoxData);
public:
    static Ref<StyleFlexibleBoxData> create() { return adoptRef(*new StyleFlexibleBoxData); }
    Ref<StyleFlexibleBoxData> copy() const;

    bool operator==(const StyleFlexibleBoxData&) const;

#if !LOG_DISABLED
    void dumpDifferences(TextStream&, const StyleFlexibleBoxData&) const;
#endif

    Style::FlexGrow flexGrow;
    Style::FlexShrink flexShrink;
    Style::FlexBasis flexBasis;

    PREFERRED_TYPE(FlexDirection) unsigned flexDirection : 2;
    PREFERRED_TYPE(FlexWrap) unsigned flexWrap : 2;

private:
    StyleFlexibleBoxData();
    StyleFlexibleBoxData(const StyleFlexibleBoxData&);
};

} // namespace WebCore
