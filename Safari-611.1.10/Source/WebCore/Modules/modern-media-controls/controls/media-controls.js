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

class MediaControls extends LayoutNode
{

    constructor({ width = 300, height = 150, layoutTraits = LayoutTraits.Unknown } = {})
    {
        super(`<div class="media-controls"></div>`);

        this._scaleFactor = 1;
        this._shouldCenterControlsVertically = false;

        this.width = width;
        this.height = height;
        this.layoutTraits = layoutTraits;

        this.playPauseButton = new PlayPauseButton(this);
        this.airplayButton = new AirplayButton(this);
        this.pipButton = new PiPButton(this);
        this.fullscreenButton = new FullscreenButton(this);
        this.muteButton = new MuteButton(this);
        this.tracksButton = new TracksButton(this);

        this.statusLabel = new StatusLabel(this);
        this.timeControl = new TimeControl(this);

        this.tracksPanel = new TracksPanel;

        this.bottomControlsBar = new ControlsBar("bottom");

        this.autoHideController = new AutoHideController(this);
        this.autoHideController.fadesWhileIdle = false;
        this.autoHideController.hasSecondaryUIAttached = false;

        this._placard = null;
        this.airplayPlacard = new AirplayPlacard(this);
        this.invalidPlacard = new InvalidPlacard(this);
        this.pipPlacard = new PiPPlacard(this);

        this.element.addEventListener("focusin", this);
        window.addEventListener("dragstart", this, true);
    }

    // Public

    get visible()
    {
        return super.visible;
    }

    set visible(flag)
    {
        if (this.visible === flag)
            return;

        // If we just got made visible again, let's fade the controls in.
        if (flag && !this.visible)
            this.faded = false;
        else if (!flag)
            this.autoHideController.mediaControlsBecameInvisible();

        super.visible = flag;

        if (flag)
            this.layout();

        if (this.delegate && typeof this.delegate.mediaControlsVisibilityDidChange === "function")
            this.delegate.mediaControlsVisibilityDidChange();
    }

    get faded()
    {
        return !!this._faded;
    }

    set faded(flag)
    {
        if (this._faded === flag)
            return;

        this._faded = flag;
        this.markDirtyProperty("faded");

        this.autoHideController.mediaControlsFadedStateDidChange();
        if (this.delegate && typeof this.delegate.mediaControlsFadedStateDidChange === "function")
            this.delegate.mediaControlsFadedStateDidChange();
    }

    get usesLTRUserInterfaceLayoutDirection()
    {
        return this.element.classList.contains("uses-ltr-user-interface-layout-direction");
    }

    set usesLTRUserInterfaceLayoutDirection(flag)
    {
        this.needsLayout = this.usesLTRUserInterfaceLayoutDirection !== flag;
        this.element.classList.toggle("uses-ltr-user-interface-layout-direction", flag);
    }

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

    get placard()
    {
        return this._placard;
    }

    set placard(placard)
    {
        if (this._placard === placard)
            return;

        this._placard = placard;
        this.layout();
    }

    placardPreventsControlsBarDisplay()
    {
        return this._placard && this._placard !== this.airplayPlacard;
    }

    showTracksPanel()
    {
        this.element.classList.add("shows-tracks-panel");

        this.tracksButton.on = true;
        this.tracksButton.element.blur();
        this.autoHideController.hasSecondaryUIAttached = true;
        this.tracksPanel.presentInParent(this);

        const controlsBounds = this.element.getBoundingClientRect();
        const controlsBarBounds = this.bottomControlsBar.element.getBoundingClientRect();
        const tracksButtonBounds = this.tracksButton.element.getBoundingClientRect();
        this.tracksPanel.rightX = this.width - (tracksButtonBounds.right - controlsBounds.left);
        this.tracksPanel.bottomY = this.height - (controlsBarBounds.top - controlsBounds.top) + 1;
        this.tracksPanel.maxHeight = this.height - this.tracksPanel.bottomY - 10;
    }

    hideTracksPanel()
    {
        this.element.classList.remove("shows-tracks-panel");

        let shouldFadeControlsBar = true;
        if (window.event instanceof MouseEvent)
            shouldFadeControlsBar = !this.isPointInControls(new DOMPoint(event.clientX, event.clientY), true);

        this.tracksButton.on = false;
        this.tracksButton.element.focus();
        this.autoHideController.hasSecondaryUIAttached = false;
        this.faded = this.autoHideController.fadesWhileIdle && shouldFadeControlsBar;
        this.tracksPanel.hide();
    }

    fadeIn()
    {
        this.element.classList.add("fade-in");
    }

    isPointInControls(point, includeContainer)
    {
        let ancestor = this.element.parentNode;
        while (ancestor && !(ancestor instanceof ShadowRoot))
            ancestor = ancestor.parentNode;

        const shadowRoot = ancestor;
        if (!shadowRoot)
            return false;

        const tappedElement = shadowRoot.elementFromPoint(point.x, point.y);

        if (includeContainer && this.element === tappedElement)
            return true;

        return this.children.some(child => child.element.contains(tappedElement));
    }

    // Protected

    handleEvent(event)
    {
        if (event.type === "focusin" && event.currentTarget === this.element)
            this.faded = false;
        else if (event.type === "dragstart" && this.isPointInControls(new DOMPoint(event.clientX, event.clientY)))
            event.preventDefault();
    }

    layout()
    {
        super.layout();

        if (this._placard) {
            this._placard.width = this.width;
            this._placard.height = this.height;
        }
    }

    commitProperty(propertyName)
    {
        if (propertyName === "scaleFactor") {
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
        } else if (propertyName === "faded")
            this.element.classList.toggle("faded", this.faded);
        else
            super.commitProperty(propertyName);
    }

}
