/*
 * Copyright (C) 2008-2017 Apple Inc. All Rights Reserved.
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

#pragma once

#include "ThemeTypes.h"
#include <optional>
#include <wtf/Forward.h>

namespace WebCore {

class Color;
class ControlStates;
class FloatRect;
class FloatSize;
class FontCascade;
class FontCascadeDescription;
class GraphicsContext;
class LengthBox;
class ScrollView;

struct LengthSize;

class Theme {
public:
    static Theme& singleton();

    // A function to obtain the baseline position adjustment for a "leaf" control. This will be used if a baseline
    // position cannot be determined by examining child content. Checkboxes and radio buttons are examples of
    // controls that need to do this. The adjustment is an offset that adds to the baseline, e.g., marginTop() + height() + |offset|.
    // The offset is not zoomed.
    virtual int baselinePositionAdjustment(ControlPartType) const;

    // The font description result should have a zoomed font size.
    virtual std::optional<FontCascadeDescription> controlFont(ControlPartType, const FontCascade&, float zoomFactor) const;

    // The size here is in zoomed coordinates already. If a new size is returned, it also needs to be in zoomed coordinates.
    virtual LengthSize controlSize(ControlPartType, const FontCascade&, const LengthSize& zoomedSize, float zoomFactor) const;

    // Returns the minimum size for a control in zoomed coordinates.
    LengthSize minimumControlSize(ControlPartType, const FontCascade&, const LengthSize& zoomedSize, const LengthSize& nonShrinkableZoomedSize, float zoomFactor) const;
    
    // Allows the theme to modify the existing padding/border.
    virtual LengthBox controlPadding(ControlPartType, const FontCascade&, const LengthBox& zoomedBox, float zoomFactor) const;
    virtual LengthBox controlBorder(ControlPartType, const FontCascade&, const LengthBox& zoomedBox, float zoomFactor) const;

    // Whether or not whitespace: pre should be forced on always.
    virtual bool controlRequiresPreWhiteSpace(ControlPartType) const;

    // Method for painting a control. The rect is in zoomed coordinates.
    // FIXME: <https://webkit.org/b/231637> Move parameters to a struct.
    virtual void paint(ControlPartType, ControlStates&, GraphicsContext&, const FloatRect& zoomedRect, float zoomFactor, ScrollView*, float deviceScaleFactor, float pageScaleFactor, bool useSystemAppearance, bool useDarkAppearance, const Color& tintColor);

    // Some controls may spill out of their containers (e.g., the check on an OS X checkbox).  When these controls repaint,
    // the theme needs to communicate this inflated rect to the engine so that it can invalidate the whole control.
    // The rect passed in is in zoomed coordinates, so the inflation should take that into account and make sure the inflation
    // amount is also scaled by the zoomFactor.
    virtual void inflateControlPaintRect(ControlPartType, const ControlStates&, FloatRect& zoomedRect, float zoomFactor) const;

    virtual void drawNamedImage(const String&, GraphicsContext&, const FloatSize&) const;

    virtual bool userPrefersContrast() const;
    virtual bool userPrefersReducedMotion() const;

protected:
    Theme() = default;
    virtual ~Theme() = default;

    virtual LengthSize minimumControlSize(ControlPartType, const FontCascade&, const LengthSize& zoomedSize, float zoomFactor) const;

private:
    Theme(const Theme&) = delete;
    void operator=(const Theme&) = delete;
};

} // namespace WebCore
