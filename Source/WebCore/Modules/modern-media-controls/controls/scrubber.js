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

class Scrubber extends Slider
{

    constructor(layoutDelegate)
    {
        super({
            cssClassName: "scrubber",
            layoutDelegate
        });

         this.height = 23;

        // Add the element used to draw the track on iOS.
        if (this.layoutTraits & LayoutTraits.iOS)
            this.addChild(new LayoutNode(`<div></div>`), 0);

        this._buffered = 0;
    }

    // Public

    get buffered()
    {
        return this._buffered;
    }

    set buffered(buffered)
    {
        if (this._buffered === buffered)
            return;

        this._buffered = buffered;
        this.needsLayout = true;
    }

    // Protected

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

        const layoutTraits = this.layoutTraits;
        if (layoutTraits & LayoutTraits.macOS)
            this._drawMacOS(ctx, width, height);
        else if (layoutTraits & LayoutTraits.iOS)
            this._drawiOS(ctx, width, height);

        ctx.restore();
    }

    _drawMacOS(ctx, width, height)
    {
        const trackHeight = 3;
        const trackY = (height - trackHeight) / 2;
        const scrubberWidth = 4;
        const scrubberHeight = height;
        const borderSize = 2;
        const scrubberPosition = Math.max(0, Math.min(width - scrubberWidth, Math.round(width * this.value)));

        // Draw background.
        ctx.save();
        ctx.beginPath();
        addRoundedRect(ctx, 0, trackY, width, trackHeight, trackHeight / 2);
        ctx.closePath();
        ctx.clip();
        ctx.fillStyle = "rgb(20, 20, 20)";
        ctx.fillRect(0, 0, width, height);
        ctx.restore();

        // Buffered.
        ctx.save();
        ctx.beginPath();
        addRoundedRect(ctx, 0, trackY, width, trackHeight, trackHeight / 2);
        ctx.closePath();
        ctx.clip();
        ctx.fillStyle = "rgb(39, 39, 39)";
        ctx.fillRect(0, 0, width * this._buffered, height);
        ctx.restore();

        // Draw played section.
        ctx.save();
        ctx.beginPath();
        addRoundedRect(ctx, 0, trackY, width, trackHeight, trackHeight / 2);
        ctx.closePath();
        ctx.clip();
        ctx.fillStyle = "rgb(84, 84, 84)";
        ctx.fillRect(0, 0, width * this.value, height);
        ctx.restore();

        // Draw the scrubber.
        ctx.save();
        ctx.clearRect(scrubberPosition - 1, 0, scrubberWidth + borderSize, height, 0);
        ctx.beginPath();
        addRoundedRect(ctx, scrubberPosition, 0, scrubberWidth, scrubberHeight, 1);
        ctx.closePath();
        ctx.clip();
        ctx.fillStyle = "rgb(138, 138, 138)";
        ctx.fillRect(0, 0, width, height);
        ctx.restore();
    }

    _drawiOS(ctx, width, height)
    {
        const trackHeight = 3;
        const trackY = (height - trackHeight) / 2;
        const scrubberWidth = 15;
        const leftPadding = 2;
        const rightPadding = 2;
        const trackWidth = this.width - leftPadding - rightPadding - scrubberWidth;

        // Draw played section.
        ctx.save();
        ctx.beginPath();
        addRoundedRect(ctx, leftPadding, trackY, scrubberWidth / 2 + trackWidth * this.value, trackHeight, trackHeight / 2);
        ctx.closePath();
        ctx.fillStyle = "white";
        ctx.fill();
        ctx.restore();
    }

}
