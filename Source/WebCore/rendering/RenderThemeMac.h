/*
 * This file is part of the theme implementation for form controls in WebCore.
 *
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef RenderThemeMac_h
#define RenderThemeMac_h

#import "RenderThemeMacShared.h"
#import <wtf/RetainPtr.h>

namespace WebCore {

class RenderProgress;
class RenderStyle;

class RenderThemeMac : public RenderThemeMacShared {
public:
    static PassRefPtr<RenderTheme> create();

    virtual NSView* documentViewFor(RenderObject*) const;

protected:
    RenderThemeMac();
    virtual ~RenderThemeMac();

#if ENABLE(VIDEO)
    virtual bool paintMediaFullscreenButton(RenderObject*, const PaintInfo&, const IntRect&);
    virtual bool paintMediaPlayButton(RenderObject*, const PaintInfo&, const IntRect&);
    virtual bool paintMediaMuteButton(RenderObject*, const PaintInfo&, const IntRect&);
    virtual bool paintMediaSeekBackButton(RenderObject*, const PaintInfo&, const IntRect&);
    virtual bool paintMediaSeekForwardButton(RenderObject*, const PaintInfo&, const IntRect&);
    virtual bool paintMediaSliderTrack(RenderObject*, const PaintInfo&, const IntRect&);
    virtual bool paintMediaSliderThumb(RenderObject*, const PaintInfo&, const IntRect&);
    virtual bool paintMediaRewindButton(RenderObject*, const PaintInfo&, const IntRect&);
    virtual bool paintMediaReturnToRealtimeButton(RenderObject*, const PaintInfo&, const IntRect&);
    virtual bool paintMediaToggleClosedCaptionsButton(RenderObject*, const PaintInfo&, const IntRect&);
    virtual bool paintMediaControlsBackground(RenderObject*, const PaintInfo&, const IntRect&);
    virtual bool paintMediaCurrentTime(RenderObject*, const PaintInfo&, const IntRect&);
    virtual bool paintMediaTimeRemaining(RenderObject*, const PaintInfo&, const IntRect&);
    virtual bool paintMediaVolumeSliderContainer(RenderObject*, const PaintInfo&, const IntRect&);
    virtual bool paintMediaVolumeSliderTrack(RenderObject*, const PaintInfo&, const IntRect&);
    virtual bool paintMediaVolumeSliderThumb(RenderObject*, const PaintInfo&, const IntRect&);
    virtual bool paintMediaFullScreenVolumeSliderTrack(RenderObject*, const PaintInfo&, const IntRect&) OVERRIDE;
    virtual bool paintMediaFullScreenVolumeSliderThumb(RenderObject*, const PaintInfo&, const IntRect&) OVERRIDE;

    // Media controls
    virtual String extraMediaControlsStyleSheet();
#if ENABLE(FULLSCREEN_API)
    virtual String extraFullScreenStyleSheet();
#endif

    virtual bool hasOwnDisabledStateHandlingFor(ControlPart) const;
    virtual bool usesMediaControlStatusDisplay();
    virtual bool usesMediaControlVolumeSlider() const;
    virtual void adjustMediaSliderThumbSize(RenderStyle*) const;
    virtual IntPoint volumeSliderOffsetFromMuteButton(RenderBox*, const IntSize&) const OVERRIDE;
#endif
};

} // namespace WebCore

#endif // RenderThemeMac_h
