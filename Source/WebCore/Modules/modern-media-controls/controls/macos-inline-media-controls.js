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

const MinimumHeightToShowVolumeSlider = 136;

class MacOSInlineMediaControls extends InlineMediaControls
{

    constructor(options = {})
    {
        options.layoutTraits = LayoutTraits.macOS;

        super(options);

        this.element.classList.add("mac");

        this._backgroundClickDelegateNotifier = new BackgroundClickDelegateNotifier(this);

        this.volumeSlider = new Slider("volume");
        this.volumeSlider.width = 60;

        this._volumeSliderContainer = new LayoutNode(`<div class="volume-slider-container"></div>`);
        this._volumeSliderContainer.children = [new BackgroundTint, this.volumeSlider];

        // Wire up events to display the volume slider.
        this.muteButton.element.addEventListener("mouseenter", this);
        this.muteButton.element.addEventListener("mouseleave", this);
        this._volumeSliderContainer.element.addEventListener("mouseleave", this);
    }

    // Protected

    layout()
    {
        super.layout();

        if (!this._volumeSliderContainer)
            return;

        this._volumeSliderContainer.x = this.rightContainer.x + this.muteButton.x;
        this._volumeSliderContainer.y = this.bottomControlsBar.y - BottomControlsBarHeight - InsideMargin;
    }

    get preferredMuteButtonStyle()
    {
        return (this.height >= MinimumHeightToShowVolumeSlider) ? Button.Styles.Bar : super.preferredMuteButtonStyle;
    }

    handleEvent(event)
    {
        if (event.type === "mouseenter" && event.currentTarget === this.muteButton.element) {
            if (this.muteButton.parent === this.rightContainer)
                this.addChild(this._volumeSliderContainer);
        } else if (event.type === "mouseleave" && (event.currentTarget === this.muteButton.element || event.currentTarget === this._volumeSliderContainer.element)) {
            if (!this._volumeSliderContainer.element.contains(event.relatedTarget))
                this._volumeSliderContainer.remove();
        } else
            super.handleEvent(event);
    }

}
