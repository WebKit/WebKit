/*
 * Copyright (C) 2022 Apple Inc. All Rights Reserved.
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

#if PLATFORM(MAC)

#import "LengthBox.h"
#import "PlatformControl.h"

namespace WebCore {

class ControlFactoryMac;
class FloatSize;
class GraphicsContext;
class IntSize;

class ControlMac : public PlatformControl {
public:
    ControlMac(ControlPart&, ControlFactoryMac&);

protected:
    static bool userPrefersContrast();
    static FloatRect inflatedRect(const FloatRect& bounds, const FloatSize&, const IntOutsets&, const ControlStyle&);

    static void updateCheckedState(NSCell *, const ControlStyle&);
    static void updateEnabledState(NSCell *, const ControlStyle&);
    static void updateFocusedState(NSCell *, const ControlStyle&);
    static void updatePressedState(NSCell *, const ControlStyle&);

    virtual IntSize cellSize(NSControlSize, const ControlStyle&) const { return { }; };
    virtual IntOutsets cellOutsets(NSControlSize, const ControlStyle&) const { return { }; };

    NSControlSize controlSizeForFont(const ControlStyle&) const;
    NSControlSize controlSizeForSystemFont(const ControlStyle&) const;
    NSControlSize controlSizeForSize(const FloatSize&, const ControlStyle&) const;

    IntSize sizeForSystemFont(const ControlStyle&) const;

    void setFocusRingClipRect(const FloatRect& clipBounds) override;

    void updateCellStates(const FloatRect&, const ControlStyle&) override;

    void drawCell(GraphicsContext&, const FloatRect&, float deviceScaleFactor, const ControlStyle&, NSCell *, NSView *, bool drawCell = true);

#if ENABLE(DATALIST_ELEMENT)
    void drawListButton(GraphicsContext&, const FloatRect&, float deviceScaleFactor, const ControlStyle&);
#endif

    ControlFactoryMac& m_controlFactory;
};

} // namespace WebCore

#endif // PLATFORM(MAC)
