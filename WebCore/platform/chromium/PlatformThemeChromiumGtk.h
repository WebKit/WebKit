/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PlatformThemeChromiumGtk_h
#define PlatformThemeChromiumGtk_h

#include "PlatformContextSkia.h"
#include "SkColor.h"
#include "SkScalar.h"
#include "ThemeTypes.h"

namespace WebCore {

class PlatformThemeChromiumGtk {
public:
    enum ArrowDirection {
        North,
        East,
        South,
        West,
    };

    static void setScrollbarColors(unsigned inactiveColor,
                                   unsigned activeColor,
                                   unsigned trackColor);
    static unsigned thumbInactiveColor() { return s_thumbInactiveColor; }
    static unsigned thumbActiveColor() { return s_thumbActiveColor; }
    static unsigned trackColor() { return s_trackColor; }

    static SkColor saturateAndBrighten(const SkScalar hsv[3], SkScalar saturateAmount, SkScalar brightenAmount);
    static SkColor outlineColor(const SkScalar hsv1[3], const SkScalar hsv2[3]);
    static void paintArrowButton(GraphicsContext*, const IntRect&, ArrowDirection, ControlStates);

private:
    PlatformThemeChromiumGtk() {}

    static unsigned s_thumbInactiveColor;
    static unsigned s_thumbActiveColor;
    static unsigned s_trackColor;
};

} // namespace WebCore

#endif // PlatformThemeChromiumGtk_h
