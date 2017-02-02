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

        this.element.classList.add("ios");
        this.element.classList.add("inline");

        this._leftContainer = new ButtonsContainer({
            buttons: [this.playPauseButton, this.skipBackButton],
            cssClassName: "left"
        });

        this._rightContainer = new ButtonsContainer({
            buttons: [this.airplayButton, this.pipButton, this.fullscreenButton],
            cssClassName: "right"
        });

        this.controlsBar.children = [this._leftContainer, this._rightContainer];

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

    // Public

    layout()
    {
        super.layout();

        // Reset dropped buttons.
        for (let button of this._rightContainer.buttons)
            delete button.dropped;

        this._leftContainer.layout();
        this._rightContainer.layout();

        this.timeControl.width = this.width - this._leftContainer.width - this._rightContainer.width;

        if (this.timeControl.isSufficientlyWide) {
            this.controlsBar.insertBefore(this.timeControl, this._rightContainer);
            this.timeControl.x = this._leftContainer.width;
        } else {
            this.timeControl.remove();
            // Since we don't have enough space to display the scrubber, we may also not have
            // enough space to display all buttons in the left and right containers, so gradually drop them.
            for (let control of [this.airplayButton, this.pipButton, this.skipBackButton, this.fullscreenButton]) {
                // Nothing left to do if the combined container widths is shorter than the available width.
                if (this._leftContainer.width + this._rightContainer.width < this.width)
                    break;

                // If the control was already not participating in layout, we can skip it.
                if (!control.visible)
                    continue;

                // This control must now be dropped.
                control.dropped = true;

                this._leftContainer.layout();
                this._rightContainer.layout();
            }
        }
    }

}

IOSInlineMediaControls.MinimumScaleToEnterFullscreen = 1.5;
