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

const ButtonMarginForThreeButtonsOrLess = 24;
const ButtonMarginForFourButtons = 16;
const ButtonMarginForFiveButtons = 12;
const FullscreenTimeControlWidth = 448;

class MacOSFullscreenMediaControls extends MediaControls
{

    constructor(options = {})
    {
        options.layoutTraits = LayoutTraits.macOS | LayoutTraits.Fullscreen;

        super(options);

        this.element.classList.add("mac");
        this.element.classList.add("fullscreen");

        // Set up fullscreen-specific buttons.
        this.volumeDownButton = new VolumeDownButton(this);
        this.volumeUpButton = new VolumeUpButton(this);
        this.rewindButton = new RewindButton(this);
        this.forwardButton = new ForwardButton(this);
        this.fullscreenButton.isFullscreen = true;

        this.volumeSlider = new Slider("volume");
        this.volumeSlider.width = 60;

        this._leftContainer = new ButtonsContainer({
            children: this._volumeControlsForCurrentDirection(),
            cssClassName: "left",
            leftMargin: 12,
            rightMargin: 0,
            buttonMargin: 6
        });

        this._centerContainer = new ButtonsContainer({
            children: [this.rewindButton, this.playPauseButton, this.forwardButton],
            cssClassName: "center",
            leftMargin: 27,
            rightMargin: 27,
            buttonMargin: 27
        });

        this._rightContainer = new ButtonsContainer({
            children: [this.airplayButton, this.pipButton, this.tracksButton, this.fullscreenButton],
            cssClassName: "right",
            leftMargin: 12,
            rightMargin: 12
        });

        this.bottomControlsBar.children = [this._leftContainer, this._centerContainer, this._rightContainer];

        this.bottomControlsBar.element.addEventListener("mousedown", this);

        this._backgroundClickDelegateNotifier = new BackgroundClickDelegateNotifier(this);
    }

    // Protected

    handleEvent(event)
    {
        if (event.type === "mousedown" && event.currentTarget === this.bottomControlsBar.element)
            this._handleMousedown(event);
        else if (event.type === "mousemove" && event.currentTarget === this.element)
            this._handleMousemove(event);
        else if (event.type === "mouseup" && event.currentTarget === this.element)
            this._handleMouseup(event);
        else
            super.handleEvent(event);
    }

    layout()
    {
        super.layout();

        const children = [];

        if (this.placard) {
            children.push(this.placard);
            if (this.placardPreventsControlsBarDisplay()) {
                this.children = children;
                return;
            }
        }

        children.push(this.bottomControlsBar);

        if (!this._rightContainer)
            return;

        const numberOfEnabledButtons = this._rightContainer.children.filter(button => button.enabled).length;

        let buttonMargin = ButtonMarginForFiveButtons;
        if (numberOfEnabledButtons === 4)
            buttonMargin = ButtonMarginForFourButtons;
        else if (numberOfEnabledButtons <= 3)
            buttonMargin = ButtonMarginForThreeButtonsOrLess;

        this._rightContainer.buttonMargin = buttonMargin;

        this._leftContainer.visible = this.muteButton.enabled;
        this._leftContainer.children = this._volumeControlsForCurrentDirection();

        this._leftContainer.layout();
        this._centerContainer.layout();
        this._rightContainer.layout();

        if (this.statusLabel.enabled && this.statusLabel.parent !== this.bottomControlsBar) {
            this.timeControl.remove();
            this.bottomControlsBar.addChild(this.statusLabel);
        } else if (!this.statusLabel.enabled && this.timeControl.parent !== this.bottomControlsBar) {
            this.statusLabel.remove();
            this.bottomControlsBar.addChild(this.timeControl);
            this.timeControl.width = FullscreenTimeControlWidth;
        }

        this.children = children;
    }

    // Private

    _volumeControlsForCurrentDirection()
    {
        return this.usesLTRUserInterfaceLayoutDirection ? [this.volumeDownButton, this.volumeSlider, this.volumeUpButton] : [this.volumeUpButton, this.volumeSlider, this.volumeDownButton];
    }

    _handleMousedown(event)
    {
        // We don't allow dragging when the interaction is initiated on an interactive element. 
        if (event.target.localName === "button" || event.target.parentNode.localName === "button" || event.target.localName === "input")
            return;

        event.preventDefault();
        event.stopPropagation();

        this._lastDragPoint = this._pointForEvent(event);

        this.element.addEventListener("mousemove", this, true);
        this.element.addEventListener("mouseup", this, true);
    }

    _handleMousemove(event)
    {
        event.preventDefault();

        const currentDragPoint = this._pointForEvent(event);

        this.bottomControlsBar.translation = new DOMPoint(
            this.bottomControlsBar.translation.x + currentDragPoint.x - this._lastDragPoint.x,
            this.bottomControlsBar.translation.y + currentDragPoint.y - this._lastDragPoint.y
        );

        this._lastDragPoint = currentDragPoint;
    }

    _handleMouseup(event)
    {
        event.preventDefault();

        delete this._lastDragPoint;

        this.element.removeEventListener("mousemove", this, true);
        this.element.removeEventListener("mouseup", this, true);
    }

    _pointForEvent(event)
    {
        return new DOMPoint(event.clientX, event.clientY);
    }

}
