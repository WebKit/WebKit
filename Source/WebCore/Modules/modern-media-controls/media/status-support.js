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

class StatusSupport extends MediaControllerSupport
{

    // Protected

    get control()
    {
        return this.mediaController.controls.statusLabel;
    }

    get mediaEvents()
    {
        return ["durationchange", "loadstart", "error", "abort", "suspend", "stalled", "waiting", "playing", "emptied", "loadedmetadata", "loadeddata", "canplay", "canplaythrough"];
    }

    syncControl()
    {
        const media = this.mediaController.media;
        const isLiveBroadcast = media.duration === Number.POSITIVE_INFINITY;
        const canPlayThrough = media.readyState === HTMLMediaElement.HAVE_ENOUGH_DATA && !media.error;

        if (!!media.error)
            this.control.text = UIString("Error");
        else if (isLiveBroadcast && media.readyState >= HTMLMediaElement.HAVE_CURRENT_DATA)
            this.control.text = UIString("Live Broadcast");
        else
            this.control.text = "";

        this.mediaController.controls.timeControl.loading = !media.played.length && !canPlayThrough && media.networkState === HTMLMediaElement.NETWORK_LOADING;
    }

}
