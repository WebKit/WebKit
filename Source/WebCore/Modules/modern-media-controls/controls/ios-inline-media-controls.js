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

class IOSInlineMediaControls extends MediaControls
{

    constructor(options = {})
    {
        options.layoutTraits = LayoutTraits.iOS;

        super(options);

        this._inlineLayoutSupport = new InlineLayoutSupport(this, [this.airplayButton, this.pipButton, this.skipBackButton, this.fullscreenButton]);

        this.element.classList.add("ios");
        this.element.classList.add("inline");

        this.leftContainer = new ButtonsContainer({
            buttons: [this.playPauseButton, this.skipBackButton],
            cssClassName: "left"
        });

        this.rightContainer = new ButtonsContainer({
            buttons: [this.airplayButton, this.pipButton, this.fullscreenButton],
            cssClassName: "right"
        });

        this.layoutTraitsDidChange();

        this.controlsBar.children = [this.leftContainer, this.rightContainer];

        this._pinchGestureRecognizer = new PinchGestureRecognizer(this.element, this);
    }

    // Protected

    gestureRecognizerStateDidChange(recognizer)
    {
        if (this._pinchGestureRecognizer !== recognizer)
            return;

        if (recognizer.state !== GestureRecognizer.States.Ended && recognizer.state !== GestureRecognizer.States.Changed)
            return;

        if (recognizer.scale > IOSInlineMediaControls.MinimumScaleToEnterFullscreen && this.delegate && typeof this.delegate.iOSInlineMediaControlsRecognizedPinchInGesture === "function")
            this.delegate.iOSInlineMediaControlsRecognizedPinchInGesture();
    }

    // Protected

    layout()
    {
        super.layout();

        if (this.controlsBar.visible)
            this.controlsBar.children = this._inlineLayoutSupport.childrenAfterPerformingLayout();
    }

    layoutTraitsDidChange()
    {
        if (!this.leftContainer || !this.rightContainer)
            return;

        const margin = (this.layoutTraits & LayoutTraits.TightPadding) ? 12 : 24;
        this.leftContainer.leftMargin = margin;
        this.leftContainer.rightMargin = margin;
        this.leftContainer.buttonMargin = margin;
        this.rightContainer.leftMargin = margin;
        this.rightContainer.rightMargin = margin;
        this.rightContainer.buttonMargin = margin;
    }

}

IOSInlineMediaControls.MinimumScaleToEnterFullscreen = 1.5;
