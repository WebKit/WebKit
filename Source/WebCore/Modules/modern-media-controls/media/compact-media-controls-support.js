/*
 * Copyright (C) 2018 Apple Inc. All Rights Reserved.
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

class CompactMediaControlsSupport extends MediaControllerSupport
{

    // Protected

    get mediaEvents()
    {
        return ["pause", "error"];
    }

    handleEvent(event)
    {
        switch (event.type) {
        case "pause":
            this.mediaController.controls.state = CompactMediaControls.States.Paused;
            break;
        case "error":
            this.mediaController.controls.state = CompactMediaControls.States.Invalid;
            break;
        }
    }

    enable()
    {
        super.enable();
        
        for (let button of this._buttons())
            button.uiDelegate = this;
    }

    disable()
    {
        super.disable();
        
        for (let button of this._buttons())
            button.uiDelegate = null;
    }

    buttonWasPressed(button)
    {
        if (button === this.mediaController.controls.playButton) {
            this.mediaController.media.play();
            this.mediaController.controls.state = CompactMediaControls.States.Pending;
        } else if (button === this.mediaController.controls.activityIndicator) {
            this.mediaController.media.pause();
            this.mediaController.controls.state = CompactMediaControls.States.Paused;
        }
    }

    // Private

    _buttons()
    {
        return [this.mediaController.controls.playButton, this.mediaController.controls.activityIndicator];
    }

}
