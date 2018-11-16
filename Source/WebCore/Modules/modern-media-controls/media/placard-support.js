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

class PlacardSupport extends MediaControllerSupport
{

    constructor(mediaController)
    {
        super(mediaController);
        this._updatePlacard();
    }

    // Protected

    get mediaEvents()
    {
        return ["loadstart", "error", "webkitpresentationmodechanged", "webkitcurrentplaybacktargetiswirelesschanged"];
    }

    handleEvent(event)
    {
        this._updatePlacard();
    }

    disable()
    {
        // We should not allow disabling Placard support when playing inline as it would prevent the
        // PiP placard from being shown if the controls are disabled.
        if (this.mediaController.isFullscreen)
            super.disable();
    }

    // Private

    _updatePlacard()
    {
        const controls = this.mediaController.controls;
        const media = this.mediaController.media;

        let placard = null;
        if (media.webkitPresentationMode === "picture-in-picture")
            placard = controls.pipPlacard;
        else if (media.webkitCurrentPlaybackTargetIsWireless) {
            this._updateAirPlayPlacard();
            placard = controls.airplayPlacard;
        } else if (media instanceof HTMLVideoElement && media.error !== null && media.played.length === 0)
            placard = controls.invalidPlacard;

        controls.placard = placard;
    }
    
    _updateAirPlayPlacard()
    {
        var deviceName = "";
        
        if (!this.mediaController.host)
            return;
        
        switch(this.mediaController.host.externalDeviceType) {
            case 'airplay':
                deviceName = UIString("This video is playing on “%s”.", this.mediaController.host.externalDeviceDisplayName || UIString("Apple TV"));
                break;
            case 'tvout':
                deviceName = UIString("This video is playing on the TV.");
                break;
        }
        this.mediaController.controls.airplayPlacard.description = deviceName;
    }

}
