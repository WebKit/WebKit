/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#if ENABLE(ATTACHMENT_ELEMENT) && PLATFORM(COCOA)

#include "FloatRect.h"
#include "RenderAttachment.h"
#include <wtf/RetainPtr.h>

OBJC_CLASS NSDictionary;

namespace WebCore {

class Image;

enum class AttachmentLayoutStyle : uint8_t { NonSelected, Selected };
struct AttachmentLayout {
    explicit AttachmentLayout(const RenderAttachment&, AttachmentLayoutStyle style = AttachmentLayoutStyle::NonSelected);
    
    float widthPadding { 0 };
    CGFloat wrappingWidth { 0 };
    FloatRect iconRect;
    FloatRect iconBackgroundRect;
    FloatRect attachmentRect;
    FloatRect progressRect;
    AttachmentLayoutStyle style;
    float progress { 0 };
    bool excludeTypographicLeading { false };
    RefPtr<Image> icon;
    RefPtr<Image> thumbnailIcon;
    Vector<CGPoint> origins;
    int baseline { 0 };
    bool hasProgress { false };
    
    struct LabelLine {
        FloatRect rect;
        FloatRect backgroundRect;
        RetainPtr<CTLineRef> line;
        RetainPtr<CTFontRef> font;
    };
    
    FloatRect subtitleTextRect;
    
    Vector<LabelLine> lines;
    
    CGFloat contentYOrigin { 0 };
    void layOutSubtitle(const RenderAttachment& attachment);
    void layOutTitle(const RenderAttachment& attachment);
    void buildWrappedLines(String& text, CTFontRef font, NSDictionary* textAttributes, unsigned maximumLineCount);
    void buildSingleLine(const String& text, CTFontRef font, NSDictionary* textAttributes);
    void addLine(CTFontRef font, CTLineRef line, bool isSubtitle);
};

} // namespace WebCore

#endif // ENABLE(ATTACHMENT_ELEMENT) && PLATFORM(COCOA)
