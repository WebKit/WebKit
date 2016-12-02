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

        this._leftContainer = new ButtonsContainer({
            buttons: [this.playPauseButton, this.skipBackButton],
            cssClassName: "left",
            padding: 24,
            margin: 24
        });

        this._rightContainer = new ButtonsContainer({
            buttons: [this.muteButton, this.airplayButton, this.pipButton, this.tracksButton, this.fullscreenButton],
            cssClassName: "right",
            padding: 24,
            margin: 24
        });

        this._volumeSliderContainer = new LayoutNode(`<div class="volume-slider-container">`);
        this._volumeSliderContainer.children = [this.volumeSlider];
        this._volumeSliderContainer.visible = false;
        this.volumeSlider.width = 60;

        // Wire up events to display the volume slider.
        this.muteButton.element.addEventListener("mouseenter", this);
        this.muteButton.element.addEventListener("mouseleave", this);
        this._volumeSliderContainer.element.addEventListener("mouseleave", this);

        this.controlsBar.children = [this._leftContainer, this._rightContainer, this._volumeSliderContainer];
    }

    // Public

    layout()
    {
        super.layout();

        if (!this.controlsBar.visible)
            return;

        // Reset dropped buttons.
        this._rightContainer.buttons.concat(this._leftContainer.buttons).forEach(button => delete button.dropped);

        this._leftContainer.layout();
        this._rightContainer.layout();

        const middleContainer = !!this.statusLabel.text ? this.statusLabel : this.timeControl;
        this.controlsBar.children = [this._leftContainer, middleContainer, this._rightContainer, this._volumeSliderContainer];

        if (middleContainer === this.timeControl)
            this.timeControl.width = this.width - this._leftContainer.width - this._rightContainer.width;

        if (middleContainer === this.timeControl && this.timeControl.isSufficientlyWide)
            this.timeControl.x = this._leftContainer.width;
        else {
            this.timeControl.remove();

            let droppedControls = false;

            // Since we don't have enough space to display the scrubber, we may also not have
            // enough space to display all buttons in the left and right containers, so gradually drop them.
            for (let button of [this.airplayButton, this.pipButton, this.tracksButton, this.muteButton, this.skipBackButton, this.fullscreenButton]) {
                // Nothing left to do if the combined container widths is shorter than the available width.
                if (this._leftContainer.width + this._rightContainer.width < this.width)
                    break;

                droppedControls = true;

                // If the button was already not participating in layout, we can skip it.
                if (!button.visible)
                    continue;

                // This button must now be dropped.
                button.dropped = true;

                this._leftContainer.layout();
                this._rightContainer.layout();
            }

            // We didn't need to drop controls and we have status text to show.
            if (!droppedControls && middleContainer === this.statusLabel) {
                this.statusLabel.x = this._leftContainer.width;
                this.statusLabel.width = this.width - this._leftContainer.width - this._rightContainer.width;
            }
        }

        this._rightContainer.x = this.width - this._rightContainer.width;
        this._volumeSliderContainer.x = this._rightContainer.x + this.muteButton.x;
    }

    showTracksPanel()
    {
        super.showTracksPanel();
        this.tracksPanel.rightX = this._rightContainer.width - this.tracksButton.x - this.tracksButton.width;
    }

    // Protected

    handleEvent(event)
    {
        this._volumeSliderContainer.visible = event.type === "mouseenter" || event.relatedTarget === this._volumeSliderContainer.element;
    }

}
