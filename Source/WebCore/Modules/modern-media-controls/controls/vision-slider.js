/*
 * Copyright (C) 2022-2023 Apple Inc. All rights reserved.
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

class VisionSlider extends SliderBase
{
    constructor(layoutDelegate, cssClassName = "")
    {
        super(layoutDelegate, `vision ${cssClassName}`);

        this._recessed = false;

        this._gutter = new LayoutNode(`<div class="gutter"><div class="background"></div><div class="inner-shadow"></div><div class="outer-shadow"></div><div class="highlight"></div></div>`);
        this._primaryFill = new LayoutNode(`<div class="fill"></div>`);
        this.appearanceContainer.children = [this._gutter, this._primaryFill];
    }

    // Protected

    get recessed()
    {
        return this._recessed;
    }

    set recessed(recessed)
    {
        if (recessed == this._recessed)
            return;

        this._recessed = recessed;
        this.element.classList.toggle("recessed", recessed);
        this.needsLayout = true;
    }

    get _recessedMargin()
    {
        return this.computedValueForStylePropertyInPx("--slider-fill-recessed-margin");
    }

    get _effectiveSliderHeight()
    {
        // We can't use --slider-fill-height here because the calc() doesn't get resolved.
        return this.computedValueForStylePropertyInPx("--slider-height") - 2 * this._recessedMargin;
    }

    commit()
    {
        super.commit();

        const margin = this._recessedMargin;
        const height = this._effectiveSliderHeight;
        const fillX = margin;
        const fillWidth = this.value * (this.width - (margin * 2) - height) + height;
        this._primaryFill.element.style.left = `${fillX}px`;
        this._primaryFill.element.style.width = `${fillWidth}px`;
    }
}
