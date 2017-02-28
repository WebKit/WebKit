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

class MacOSInlineMediaControls extends MacOSMediaControls
{

    constructor(options)
    {
        super(options);

        this._inlineLayoutSupport = new InlineLayoutSupport(this, [this.airplayButton, this.pipButton, this.tracksButton, this.muteButton, this.skipBackButton, this.fullscreenButton]);

        this.element.classList.add("inline");

        this.leftContainer = new ButtonsContainer({
            buttons: [this.playPauseButton, this.skipBackButton],
            cssClassName: "left"
        });

        this.rightContainer = new ButtonsContainer({
            cssClassName: "right"
        });

        this.layoutTraitsDidChange();

        this._backgroundTint = new BackgroundTint;

        this._volumeSliderContainer = new LayoutNode(`<div class="volume-slider-container"></div>`);
        this._volumeSliderContainer.children = [new BackgroundTint, this.volumeSlider];
        this._volumeSliderContainer.visible = false;
        this.volumeSlider.width = 60;

        // Wire up events to display the volume slider.
        this.muteButton.element.addEventListener("mouseenter", this);
        this.muteButton.element.addEventListener("mouseleave", this);
        this._volumeSliderContainer.element.addEventListener("mouseleave", this);

        this.controlsBar.children = [this._backgroundTint, this.leftContainer, this.rightContainer, this._volumeSliderContainer];
    }

    // Public

    layout()
    {
        super.layout();

        if (!this.controlsBar.visible)
            return;

        const children = this._inlineLayoutSupport.childrenAfterPerformingLayout();
        // Add the background tint as the first child.
        children.unshift(this._backgroundTint);
        children.push(this._volumeSliderContainer);
        this.controlsBar.children = children;

        this._volumeSliderContainer.x = this.rightContainer.x + this.muteButton.x;
    }

    showTracksPanel()
    {
        super.showTracksPanel();
        this.tracksPanel.rightX = this.rightContainer.width - this.tracksButton.x - this.tracksButton.width;
    }

    // Protected

    handleEvent(event)
    {
        if (event.type === "mouseenter" && event.currentTarget === this.muteButton.element)
            this._volumeSliderContainer.visible = true;
        else if (event.type === "mouseleave" && (event.currentTarget === this.muteButton.element || event.currentTarget === this._volumeSliderContainer.element))
            this._volumeSliderContainer.visible = this._volumeSliderContainer.element.contains(event.relatedTarget);
        else
            super.handleEvent(event);
    }

    controlsBarVisibilityDidChange(controlsBar)
    {
        if (controlsBar.visible)
            this.layout();
    }

    layoutTraitsDidChange()
    {
        if (!this.leftContainer || !this.rightContainer)
            return;

        const layoutTraits = this.layoutTraits;
        if (layoutTraits & LayoutTraits.Compact) {
            this.leftContainer.leftMargin = 8;
            this.leftContainer.rightMargin = 12;
            this.leftContainer.buttonMargin = 12;
            this.rightContainer.leftMargin = 12;
            this.rightContainer.rightMargin = 8;
            this.rightContainer.buttonMargin = 12;
        } else if (layoutTraits & LayoutTraits.TightPadding) {
            this.leftContainer.leftMargin = 12;
            this.leftContainer.rightMargin = 12;
            this.leftContainer.buttonMargin = 12;
            this.rightContainer.leftMargin = 12;
            this.rightContainer.rightMargin = 12;
            this.rightContainer.buttonMargin = 12;
        } else if (layoutTraits & LayoutTraits.ReducedPadding) {
            this.leftContainer.leftMargin = 12;
            this.leftContainer.rightMargin = 16;
            this.leftContainer.buttonMargin = 16;
            this.rightContainer.leftMargin = 0;
            this.rightContainer.rightMargin = 12;
            this.rightContainer.buttonMargin = 16;
        } else {
            this.leftContainer.leftMargin = 24;
            this.leftContainer.rightMargin = 24;
            this.leftContainer.buttonMargin = 24;
            this.rightContainer.leftMargin = 24;
            this.rightContainer.rightMargin = 24;
            this.rightContainer.buttonMargin = 24;
        }

        if (layoutTraits & LayoutTraits.Compact)
            this.rightContainer.buttons = [this.muteButton, this.fullscreenButton];
        else
            this.rightContainer.buttons = [this.muteButton, this.airplayButton, this.pipButton, this.tracksButton, this.fullscreenButton];

        this.leftContainer.buttons.forEach(button => button.layoutTraitsDidChange());
        this.rightContainer.buttons.forEach(button => button.layoutTraitsDidChange());

        this.element.classList.toggle("compact", layoutTraits & LayoutTraits.Compact);
    }

}
