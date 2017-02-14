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
        return ["error", "webkitpresentationmodechanged", "webkitcurrentplaybacktargetiswirelesschanged"];
    }

    handleEvent(event)
    {
        this._updatePlacard();
    }

    // Private

    _updatePlacard()
    {
        const controls = this.mediaController.controls;

        const media = this.mediaController.media;
        if (media.webkitPresentationMode === "picture-in-picture")
            controls.showPlacard(controls.pipPlacard);
        else if (media.webkitCurrentPlaybackTargetIsWireless)
            controls.showPlacard(controls.airplayPlacard);
        else if (media instanceof HTMLVideoElement && media.error !== null && media.played.length === 0)
            controls.showPlacard(controls.invalidPlacard);
        else
            controls.hidePlacard();
    }

}
