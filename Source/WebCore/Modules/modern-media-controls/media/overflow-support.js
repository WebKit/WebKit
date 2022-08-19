/*
 * Copyright (C) 2021 Apple Inc. All Rights Reserved.
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

class OverflowSupport extends MediaControllerSupport
{

    // Protected

    get mediaEvents()
    {
        return [
            "abort",
            "canplay",
            "canplaythrough",
            "durationchange",
            "emptied",
            "error",
            "loadeddata",
            "loadedmetadata",
            "loadstart",
            "playing",
            "stalled",
            "suspend",
            "waiting",
        ];
    }

    get tracksToMonitor()
    {
        return [this.mediaController.media.textTracks];
    }

    get control()
    {
        return this.mediaController.controls.overflowButton;
    }

    buttonWasPressed(control)
    {
        this.mediaController.showMediaControlsContextMenu(control);
    }

    syncControl()
    {
        this.control.enabled = this.mediaController.canShowMediaControlsContextMenu;

        let defaultContextMenuOptions = {
            includeShowMediaStats: true,
        };

        if (this._includePlaybackRates)
            defaultContextMenuOptions.includePlaybackRates = true;

        for (let textTrack of this.mediaController.media.textTracks) {
            if (textTrack.kind !== "chapters")
                continue;

            if (textTrack.mode === "disabled")
                textTrack.mode = "hidden";
            defaultContextMenuOptions.includeChapters = true;
        }

        this.control.defaultContextMenuOptions = defaultContextMenuOptions;
    }

    // Private

    get _includePlaybackRates()
    {
        if (this.mediaController.hidePlaybackRates)
            return false;

        let media = this.mediaController.media;

        if (media.duration === Number.POSITIVE_INFINITY && media.readyState >= HTMLMediaElement.HAVE_CURRENT_DATA) {
            // Do not allow adjustment of the playback rate for live broadcasts.
            return false;
        }

        if (window.MediaStream && media.srcObject instanceof MediaStream) {
            // http://w3c.github.io/mediacapture-main/#mediastreams-in-media-elements
            // "playbackRate" - A MediaStream is not seekable.
            return false;
        }

        return true;
    }

}
