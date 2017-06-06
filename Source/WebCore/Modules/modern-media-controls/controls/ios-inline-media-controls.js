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

class IOSInlineMediaControls extends InlineMediaControls
{

    constructor(options = {})
    {
        options.layoutTraits = LayoutTraits.iOS;

        super(options);

        this.element.classList.add("ios");

        this._pinchGestureRecognizer = new PinchGestureRecognizer(this.element, this);
    }

    // Public

    get showsStartButton()
    {
        return super.showsStartButton;
    }

    set showsStartButton(flag)
    {
        super.showsStartButton = flag;

        if (!flag)
            delete this._tapGestureRecognizer;
        else if (!this._tapGestureRecognizer)
            this._tapGestureRecognizer = new TapGestureRecognizer(this.element, this);
    }

    // Protected

    gestureRecognizerStateDidChange(recognizer)
    {
        if (recognizer === this._pinchGestureRecognizer)
            this._pinchGestureRecognizerStateDidChange(recognizer);
        else if (recognizer === this._tapGestureRecognizer)
            this._tapGestureRecognizerStateDidChange(recognizer);
    }

    // Private

    _pinchGestureRecognizerStateDidChange(recognizer)
    {
        if (recognizer.state !== GestureRecognizer.States.Ended && recognizer.state !== GestureRecognizer.States.Changed)
            return;

        if (recognizer.scale > IOSInlineMediaControls.MinimumScaleToEnterFullscreen && this.delegate && typeof this.delegate.iOSInlineMediaControlsRecognizedPinchInGesture === "function")
            this.delegate.iOSInlineMediaControlsRecognizedPinchInGesture();
    }

    _tapGestureRecognizerStateDidChange(recognizer)
    {
        console.assert(this.showsStartButton);
        if (recognizer.state === GestureRecognizer.States.Recognized && this.delegate && typeof this.delegate.iOSInlineMediaControlsRecognizedTapGesture === "function")
            this.delegate.iOSInlineMediaControlsRecognizedTapGesture();
    }

}

IOSInlineMediaControls.MinimumScaleToEnterFullscreen = 1.5;
