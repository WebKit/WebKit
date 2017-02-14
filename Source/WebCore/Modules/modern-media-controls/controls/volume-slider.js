/*
 * Copyright (C) 2016 Apple Inc. All Rights Reserved.
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

class VolumeSlider extends Slider
{

    constructor(layoutDelegate)
    {
        super({
            cssClassName: "volume",
            layoutDelegate
        });

        this.height = 11;
        this.enabled = true;

        this._active = false;
        this.element.addEventListener("mousedown", this);
    }

    // Protected

    handleEvent(event)
    {
        super.handleEvent(event);

        if (event instanceof MouseEvent) {
            this._active = event.type === "mousedown";
            if (event.type === "mousedown")
                window.addEventListener("mouseup", this, true);
            else
                window.removeEventListener("mouseup", this, true);
        }
    }

    draw(ctx)
    {
        const width = this.width;
        const height = this.height;

        if (!this.width || !this.height)
            return;

        const dpr = window.devicePixelRatio;

        ctx.save();
        ctx.scale(dpr, dpr);
        ctx.clearRect(0, 0, width, height);

        const trackHeight = 3;
        const knobRadius = 5.5;
        const knobDiameter = 2 * knobRadius;
        const borderSize = 2;
        const knobX = Math.round(this.value * (width - knobDiameter - borderSize));

        // Draw portion of volume under slider thumb.
        ctx.save();
        ctx.beginPath();
        addRoundedRect(ctx, 0, 4, knobX + 2, trackHeight, trackHeight / 2);
        ctx.closePath();
        ctx.clip();
        ctx.fillStyle = "rgb(84, 84, 84)";
        ctx.fillRect(0, 0, width, height);
        ctx.restore();

        // Draw portion of volume above slider thumb.
        ctx.save();
        ctx.beginPath();
        addRoundedRect(ctx, knobX, 4, width - knobX, trackHeight, trackHeight / 2);
        ctx.closePath();
        ctx.clip();
        ctx.fillStyle = "rgb(39, 39, 39)";
        ctx.fillRect(0, 0, width, height);
        ctx.restore();

        // Clear a hole in the slider for the scrubber.
        ctx.save();
        ctx.beginPath();
        addRoundedRect(ctx, knobX, 0, knobDiameter + borderSize, height, (knobDiameter + borderSize) / 2);
        ctx.closePath();
        ctx.clip();
        ctx.clearRect(0, 0, width, height);
        ctx.restore();

        // Draw scrubber.
        ctx.save();
        ctx.beginPath();
        addRoundedRect(ctx, knobX + 1, 0, knobDiameter, knobDiameter, knobRadius);
        ctx.closePath();
        ctx.clip();
        ctx.fillStyle = this._active ? "white" : "rgb(138, 138, 138)";
        ctx.fillRect(0, 0, width, height);
        ctx.restore();

        ctx.restore();
    }    

}
