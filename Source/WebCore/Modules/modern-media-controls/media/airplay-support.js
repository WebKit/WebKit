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

class AirplaySupport extends MediaControllerSupport
{

    // Protected

    get control()
    {
        return this.mediaController.controls.airplayButton;
    }

    get mediaEvents()
    {
        return ["webkitplaybacktargetavailabilitychanged", "webkitcurrentplaybacktargetiswirelesschanged"];
    }

    enable()
    {
        if (this._shouldBeEnabled())
            super.enable();
    }

    buttonWasPressed(control)
    {
        this.mediaController.media.webkitShowPlaybackTargetPicker();
    }

    controlsUserVisibilityDidChange()
    {
        if (this._shouldBeEnabled())
            this.enable();
        else
            this.disable();
    }

    handleEvent(event)
    {
        if (event.type === "webkitplaybacktargetavailabilitychanged")
            this._routesAvailable = event.availability === "available";

        super.handleEvent(event);
    }

    syncControl()
    {
        this.control.enabled = !!this._routesAvailable;
        this.control.on = this.mediaController.media.webkitCurrentPlaybackTargetIsWireless;
        this.mediaController.controls.muteButton.enabled = !this.control.on;
    }

    // Private

    _shouldBeEnabled()
    {
        if (!this.mediaController.hasPlayed)
            return false;

        const controls = this.mediaController.controls;
        return controls.visible && !controls.faded;
    }

}
