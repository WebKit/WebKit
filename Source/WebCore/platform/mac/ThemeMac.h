/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef ThemeMac_h
#define ThemeMac_h

#include "ThemeCocoa.h"

@interface NSFont(WebCoreTheme)
- (NSString*)webCoreFamilyName;
@end

namespace WebCore {

class ThemeMac : public ThemeCocoa {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ThemeMac() { }
    virtual ~ThemeMac() { }
    
    int baselinePositionAdjustment(ControlPart) const override;

    Optional<FontCascadeDescription> controlFont(ControlPart, const FontCascade&, float zoomFactor) const override;
    
    LengthSize controlSize(ControlPart, const FontCascade&, const LengthSize&, float zoomFactor) const override;
    LengthSize minimumControlSize(ControlPart, const FontCascade&, float zoomFactor) const override;

    LengthBox controlPadding(ControlPart, const FontCascade&, const LengthBox& zoomedBox, float zoomFactor) const override;
    LengthBox controlBorder(ControlPart, const FontCascade&, const LengthBox& zoomedBox, float zoomFactor) const override;

    bool controlRequiresPreWhiteSpace(ControlPart part) const override { return part == PushButtonPart; }

    void paint(ControlPart, ControlStates&, GraphicsContext&, const FloatRect&, float zoomFactor, ScrollView*, float deviceScaleFactor, float pageScaleFactor) override;
    void inflateControlPaintRect(ControlPart, const ControlStates&, FloatRect&, float zoomFactor) const override;

    // FIXME: Once RenderThemeMac is converted over to use Theme then this can be internal to ThemeMac.
    static NSView* ensuredView(ScrollView*, const ControlStates&, bool useUnparentedView = false);
    static void setFocusRingClipRect(const FloatRect&);
    static bool drawCellOrFocusRingWithViewIntoContext(NSCell*, GraphicsContext&, const FloatRect&, NSView*, bool /* drawButtonCell */, bool /* drawFocusRing */, bool /* useImageBuffer */, float /* deviceScaleFactor */);
};

} // namespace WebCore

#endif // ThemeMac_h
