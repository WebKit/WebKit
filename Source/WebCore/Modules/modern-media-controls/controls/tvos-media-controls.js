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
    static backForwardButtonScaleFactor = 2;
    static playPauseButtonScaleFactor = 3;
    static centerControlsBarButtonMargin = 48;

    constructor(options = {})
    {
        options.layoutTraits = new TVOSLayoutTraits(LayoutTraits.Mode.Fullscreen)

        super(options)

        this.element.classList.add("fullscreen");
        this.element.classList.add("tvos");

        this.skipBackButton = new SkipBackButton(this);
        this.skipForwardButton = new SkipForwardButton(this);

        this.centerControlsBar = new ControlsBar("center");
        this._centerControlsBarContainer = this.centerControlsBar.addChild(new ButtonsContainer);

        this.showsStartButton = false;
    }

    // Protected

    layout()
    {
        super.layout();

        if (!this.centerControlsBar)
            return;

        this.playPauseButton.scaleFactor = TVOSMediaControls.playPauseButtonScaleFactor;
        this.skipForwardButton.scaleFactor = TVOSMediaControls.backForwardButtonScaleFactor;
        this.skipBackButton.scaleFactor = TVOSMediaControls.backForwardButtonScaleFactor;

        this._centerControlsBarContainer.children = this._centerContainerButtons();
        this._centerControlsBarContainer.buttonMargin = TVOSMediaControls.centerControlsBarButtonMargin;
        this._centerControlsBarContainer.layout();

        this.centerControlsBar.width = this._centerControlsBarContainer.width;
        this.centerControlsBar.visible = this._centerControlsBarContainer.children.some(button => button.visible);

        this.children = [this.centerControlsBar];
    }

    // Private

    _centerContainerButtons()
    {
        return [this.skipBackButton, this.playPauseButton, this.skipForwardButton];
    }
}
