/*
 * Copyright (C) 2018 Apple Inc. All Rights Reserved.
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

class CompactMediaControls extends LayoutNode
{

    constructor({ width = 320, height = 240 } = {})
    {
        super(`<div class="compact media-controls"></div>`);

        this._state = CompactMediaControls.States.Paused;
        this._scaleFactor = 1;
        this._shouldCenterControlsVertically = false;

        this.layoutTraits = LayoutTraits.Compact;

        this.playButton = new Button({
            cssClassName: "play",
            iconName: Icons.PlayCompact,
            layoutDelegate: this
        });

        this.invalidButton = new Button({
            cssClassName: "invalid",
            iconName: Icons.InvalidCompact,
            layoutDelegate: this
        });

        this.activityIndicator = new CompactActivityIndicator(this);

        this.width = width;
        this.height = height;
    }

    // Public

    get scaleFactor()
    {
        return this._scaleFactor;
    }

    set scaleFactor(scaleFactor)
    {
        if (this._scaleFactor === scaleFactor)
            return;

        this._scaleFactor = scaleFactor;
        this.markDirtyProperty("scaleFactor");
    }

    get shouldCenterControlsVertically()
    {
        return this._shouldCenterControlsVertically;
    }

    set shouldCenterControlsVertically(flag)
    {
        if (this._shouldCenterControlsVertically === flag)
            return;

        this._shouldCenterControlsVertically = flag;
        this.markDirtyProperty("scaleFactor");
    }

    get state()
    {
        return this._state;
    }

    set state(state)
    {
        if (this._state === state)
            return;

        this._state = state;
        this.layout();
    }

    // Protected

    layout()
    {
        super.layout();

        switch (this._state) {
        case CompactMediaControls.States.Paused:
            this.children = [this.playButton];
            break;
        case CompactMediaControls.States.Pending:
            this.children = [this.activityIndicator];
            this.activityIndicator.show();
            break;
        case CompactMediaControls.States.Invalid:
            this.children = [this.invalidButton];
            break;
        }
    }

    commitProperty(propertyName)
    {
        if (propertyName !== "scaleFactor") {
            super.commitProperty(propertyName);
            return;
        }

        const zoom = 1 / this._scaleFactor;
        // We want to maintain the controls at a constant device height. To do so, we invert the page scale
        // factor using a scale transform when scaling down, when the result will not appear pixelated and
        // where the CSS zoom property produces incorrect text rendering due to enforcing the minimum font
        // size. When we would end up scaling up, which would yield pixelation, we use the CSS zoom property
        // which will not run into the font size issue.
        if (zoom < 1) {
            this.element.style.transform = `scale(${zoom})`;
            this.element.style.removeProperty("zoom");
        } else {
            this.element.style.zoom = zoom;
            this.element.style.removeProperty("transform");
        }
        // We also want to optionally center them vertically compared to their container.
        this.element.style.top = this._shouldCenterControlsVertically ? `${(this.height / 2) * (zoom - 1)}px` : "auto"; 
    }

}

CompactMediaControls.States = {
    Paused: "paused",
    Pending: "pending",
    Invalid: "invalid"
};
