/*
 * Copyright (C) 2016, 2022 Apple Inc. All rights reserved.
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

class Slider extends SliderBase
{
    constructor(layoutDelegate, cssClassName = "", knobStyle = Slider.KnobStyle.Circle)
    {
        super(layoutDelegate, `default ${cssClassName}`);

        this._primaryFill = new LayoutNode(`<div class="primary"></div>`);
        this._trackFill = new LayoutNode(`<div class="track"></div>`);
        this._secondaryFill = new LayoutNode(`<div class="secondary"></div>`);
        this._trackFill.children = [this._secondaryFill];

        let fillContainer = new LayoutNode(`<div class="fill"></div>`);
        this._knob = new LayoutNode(`<div class="knob ${knobStyle}"></div>`);
        fillContainer.children = [this._primaryFill, this._knob, this._trackFill];

        this.appearanceContainer.children = [fillContainer];

        this._cueMarkers = [];
        this._cueMarkersContainer = new LayoutNode(`<div class="cue-markers"></div>`);
        this.children = [...this.children, this._cueMarkersContainer];

        this.height = 16;
        this._knobStyle = knobStyle;
    }

    // Public

    get knobStyle()
    {
        return this._knobStyle;
    }

    set knobStyle(knobStyle)
    {
        if (this._knobStyle === knobStyle)
            return;

        this._knob.element.classList.remove(this._knobStyle);

        this._knobStyle = knobStyle;

        this._knob.element.classList.add(this._knobStyle);

        this.needsLayout = true;
    }

    get cueMarkers()
    {
        return this._cueMarkers;
    }

    set cueMarkers(cueMarkers)
    {
        this._cueMarkers = cueMarkers || [];
        this._cueMarkersContainer.children = this._cueMarkers.map(({ fraction, time, selected, index }) => {
            const clampedFraction = Math.max(0, Math.min(1, fraction));
            const marker = new LayoutNode(`<div class="cue-marker"><div class="cue-marker-diamond"></div><div class="cue-marker-line"></div></div>`);

            marker.cueFraction = clampedFraction;
            marker.element.classList.toggle("selected", !!selected);
            marker.element.dataset.findCueIndex = index;
            marker.element.style.left = `${100 * clampedFraction}%`;
            marker.element.title = utils.formattedStringForDuration(time);
            marker.element.addEventListener("pointerdown", event => {
                event.stopPropagation();
                event.preventDefault();
                this._seekToCueMarker(clampedFraction, time);
            });
            return marker;
        });
        this._updateCueMarkerColors();
    }

    // Protected

    _seekToCueMarker(fraction, time)
    {
        this.value = fraction;
        if (!this.uiDelegate)
            return;

        if (typeof this.uiDelegate.seekToTime === "function")
            this.uiDelegate.seekToTime(time);
    }

    _updateCueMarkerColors()
    {
        for (const marker of this._cueMarkersContainer.children)
            marker.element.classList.toggle("played", marker.cueFraction <= this.value);
    }

    commit()
    {
        super.commit();

        this._primaryFill.element.style.flexGrow = 100 * this.value;
        this._trackFill.element.style.flexGrow = 100 * (1 - this.value);
        this._updateCueMarkerColors();
    }

}

Slider.KnobStyle = {
    Circle: "circle",
    Bar: "bar",
    None: "none",
    Pill: "pill",
};
