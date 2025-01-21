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

#include "ControlStyle.h"
#include "ThemeTypes.h"
#include <optional>
#include <wtf/Forward.h>

namespace WebCore {

class FloatRect;
class FloatSize;
class FontCascade;
class FontCascadeDescription;
class GraphicsContext;
class ScrollView;

struct LengthSize;

struct ThemeControlSizeInput {
    bool specifiedWidth;
    bool specifiedHeight;
};
struct ThemeControlSizeOverride {
    std::optional<float> overrideWidth;
    std::optional<float> overrideHeight;
};

struct ThemeControlMinimumSizeInput {
    bool specifiedMinWidth;
    bool specifiedMinHeight;
};
struct ThemeControlMinimumSizeOverride {
    std::optional<float> overrideMinWidth;
    std::optional<float> overrideMinHeight;
};

class Theme {
public:
    static Theme& singleton();

    // The font description result should have a zoomed font size.
    virtual std::optional<FontCascadeDescription> controlFont(StyleAppearance, const FontCascade&, float) const;

    // Returns override preferred sizes for a control in zoomed coordinates.
    virtual ThemeControlSizeOverride controlSize(StyleAppearance, const FontCascade&, const ThemeControlSizeInput&, float) const;

    // Returns override minimum sizes for a control in zoomed coordinates.
    virtual ThemeControlMinimumSizeOverride minimumControlSize(StyleAppearance, const FontCascade&, const ThemeControlMinimumSizeInput& zoomedSize, float zoomFactor) const;

    // Allows the theme to modify the existing padding/border.
    virtual LengthBox controlPadding(StyleAppearance, const FontCascade&, const LengthBox& zoomedBox, float zoomFactor) const;
    virtual LengthBox controlBorder(StyleAppearance, const FontCascade&, const LengthBox& zoomedBox, float zoomFactor) const;

    // Whether or not whitespace: pre should be forced on always.
    virtual bool controlRequiresPreWhiteSpace(StyleAppearance) const { return false; }

    virtual void drawNamedImage(const String&, GraphicsContext&, const FloatSize&) const;

    virtual bool userPrefersContrast() const { return false; }
    virtual bool userPrefersReducedMotion() const { return false; }
    virtual bool userPrefersOnOffLabels() const { return false; }

protected:
    Theme() = default;
    virtual ~Theme() = default;

private:
    Theme(const Theme&) = delete;
    void operator=(const Theme&) = delete;
};

} // namespace WebCore
