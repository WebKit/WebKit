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

        this.element.classList.add("inline");

        this.leftContainer = new ButtonsContainer({
            buttons: [this.playPauseButton, this.skipBackButton],
            cssClassName: "left"
        });

        this.rightContainer = new ButtonsContainer({
            cssClassName: "right"
        });

        this._matchLayoutTraits();

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

    get layoutTraits()
    {
        return this._layoutTraits;
    }

    set layoutTraits(layoutTraits)
    {
        if (this._layoutTraits === layoutTraits)
            return;

        this._layoutTraits = layoutTraits;
        this._matchLayoutTraits();
    }

    layout()
    {
        super.layout();

        if (!this.controlsBar.visible)
            return;

        // Reset dropped buttons.
        this.rightContainer.buttons.concat(this.leftContainer.buttons).forEach(button => delete button.dropped);

        this.leftContainer.layout();
        this.rightContainer.layout();

        const middleContainer = this.statusLabel.enabled ? this.statusLabel : this.timeControl;
        this.controlsBar.children = [this._backgroundTint, this.leftContainer, middleContainer, this.rightContainer, this._volumeSliderContainer];

        if (middleContainer === this.timeControl)
            this.timeControl.width = this.width - this.leftContainer.width - this.rightContainer.width;

        if (middleContainer === this.timeControl && this.timeControl.isSufficientlyWide)
            this.timeControl.x = this.leftContainer.width;
        else {
            this.timeControl.remove();

            let droppedControls = false;

            // Since we don't have enough space to display the scrubber, we may also not have
            // enough space to display all buttons in the left and right containers, so gradually drop them.
            for (let button of [this.airplayButton, this.pipButton, this.tracksButton, this.muteButton, this.skipBackButton, this.fullscreenButton]) {
                // Nothing left to do if the combined container widths is shorter than the available width.
                if (this.leftContainer.width + this.rightContainer.width < this.width)
                    break;

                droppedControls = true;

                // If the button was already not participating in layout, we can skip it.
                if (!button.visible)
                    continue;

                // This button must now be dropped.
                button.dropped = true;

                this.leftContainer.layout();
                this.rightContainer.layout();
            }

            // We didn't need to drop controls and we have status text to show.
            if (!droppedControls && middleContainer === this.statusLabel) {
                this.statusLabel.x = this.leftContainer.width;
                this.statusLabel.width = this.width - this.leftContainer.width - this.rightContainer.width;
            }
        }

        this.rightContainer.x = this.width - this.rightContainer.width;
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
        super.handleEvent(event);
        this._volumeSliderContainer.visible = event.type === "mouseenter" || event.relatedTarget === this._volumeSliderContainer.element;
    }

    controlsBarVisibilityDidChange(controlsBar)
    {
        if (controlsBar.visible)
            this.layout();
    }

    // Private

    _matchLayoutTraits()
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
