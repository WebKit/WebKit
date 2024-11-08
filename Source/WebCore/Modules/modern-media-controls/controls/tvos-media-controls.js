/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

class TVOSMediaControls extends MediaControls
{
    static topButtonsScaleFactor = 1.5;
    static backForwardButtonScaleFactor = 2.5;
    static playPauseButtonScaleFactor = 3;
    static overflowButtonScaleFactor = 1.5;
    static bottomControlsBarButtonMargin = 80;
    static bottomControlsBarMargin = 48;

    constructor(options = {})
    {
        options.layoutTraits = new TVOSLayoutTraits(LayoutTraits.Mode.Fullscreen)

        super(options)

        this.element.classList.add("fullscreen");
        this.element.classList.add("tvos");

        this.closeButton = new CloseButton(this);

        this.timeControl.scrubber.allowsRelativeScrubbing = true;
        this.timeControl.scrubber.knobStyle = Slider.KnobStyle.None;
        this.timeControl.timeLabelsAttachment = TimeControl.TimeLabelsAttachment.Below;

        this.skipBackButton = new SkipBackButton(this);
        this.skipForwardButton = new SkipForwardButton(this);
        
        this.overflowButton.addExtraContextMenuOptions(this.tracksButton.contextMenuOptions);

        const singleButtonContainerMargins = {
            leftMargin: 0,
            rightMargin: 0,
            buttonMargin: 0
        };

        this.topLeftControlsBar = new ControlsBar("top-left");
        this._topLeftControlsBarContainer = this.topLeftControlsBar.addChild(new ButtonsContainer(singleButtonContainerMargins));

        this.topRightControlsBar = new ControlsBar("top-right");
        this._topRightControlsBarContainer = this.topRightControlsBar.addChild(new ButtonsContainer(singleButtonContainerMargins));

        this.bottomControlsBar.addChild(this.timeControl);
        this._bottomControlsBarContainer = this.bottomControlsBar.addChild(new ButtonsContainer);

        this.overflowControlsBar = new ControlsBar("overflow");
        this._overflowControlsBarContainer = this.overflowControlsBar.addChild(new ButtonsContainer(singleButtonContainerMargins));

        this.metadataContainer = new MetadataContainer();

        this.showsStartButton = false;

        for (let clickEvent of ["click", "mousedown", "mouseup", "pointerdown", "pointerup"]) {
            this.element.addEventListener(clickEvent, (event) => {
                event.stopPropagation();
            });
        }

        this._isInitialized = true
    }

    // Protected

    layout()
    {
        super.layout();

        if (!this._isInitialized)
            return;

        this.closeButton.scaleFactor = TVOSMediaControls.topButtonsScaleFactor;
        this.muteButton.scaleFactor = TVOSMediaControls.topButtonsScaleFactor;
        this.playPauseButton.scaleFactor = TVOSMediaControls.playPauseButtonScaleFactor;
        this.skipForwardButton.scaleFactor = TVOSMediaControls.backForwardButtonScaleFactor;
        this.skipBackButton.scaleFactor = TVOSMediaControls.backForwardButtonScaleFactor;
        this.overflowButton.scaleFactor = TVOSMediaControls.overflowButtonScaleFactor;

        this._topLeftControlsBarContainer.children = this._topLeftContainerButtons();
        this._topLeftControlsBarContainer.layout();

        this._topRightControlsBarContainer.children = this._topRightContainerButtons();
        this._topRightControlsBarContainer.layout();

        this._bottomControlsBarContainer.children = this._bottomContainerButtons();
        this._bottomControlsBarContainer.buttonMargin = TVOSMediaControls.bottomControlsBarButtonMargin;
        this._bottomControlsBarContainer.layout();

        this._overflowControlsBarContainer.children = this._overflowContainerButtons();
        this._overflowControlsBarContainer.layout();

        this.topLeftControlsBar.hasBackgroundTint = false;
        this.topRightControlsBar.hasBackgroundTint = false;
        this.bottomControlsBar.hasBackgroundTint = false;
        this.overflowControlsBar.hasBackgroundTint = false;

        this.topLeftControlsBar.width = this._topLeftControlsBarContainer.width;
        this.topRightControlsBar.width = this._topRightControlsBarContainer.width;
        this.bottomControlsBar.width = this.width - 2 * TVOSMediaControls.bottomControlsBarMargin;
        this.overflowControlsBar.width = this._overflowControlsBarContainer.width;

        this.timeControl.width = this.bottomControlsBar.width;
        this.metadataContainer.width = this.bottomControlsBar.width - this.overflowControlsBar.width - ButtonsContainer.Defaults.LeftMargin;

        this.topLeftControlsBar.visible = this._topLeftControlsBarContainer.children.some(button => button.visible);
        this.topRightControlsBar.visible = this._topRightControlsBarContainer.children.some(button => button.visible);
        this.bottomControlsBar.visible = true;
        this.overflowControlsBar.visible = true;

        this.children = [this.topLeftControlsBar, this.topRightControlsBar, this.bottomControlsBar, this.metadataContainer, this.overflowControlsBar];
    }

    // Private

    _topLeftContainerButtons()
    {
        if (this.usesLTRUserInterfaceLayoutDirection)
            return [this.closeButton];
        return [this.muteButton];
    }

    _topRightContainerButtons()
    {
        if (this.usesLTRUserInterfaceLayoutDirection)
            return [this.muteButton];
        return [this.closeButton];
    }

    _bottomContainerButtons()
    {
        return [this.skipBackButton, this.playPauseButton, this.skipForwardButton];
    }

    _overflowContainerButtons()
    {
        return [this.overflowButton];
    }
}
