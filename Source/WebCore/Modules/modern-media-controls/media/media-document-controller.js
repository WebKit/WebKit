/*
 * Copyright (C) 2017 Apple Inc. All Rights Reserved.
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

class MediaDocumentController
{

    constructor(mediaController)
    {
        this.mediaController = mediaController;

        // Force the controls to look like we're loading an audio file by default.
        mediaController.controls.shouldUseAudioLayout = true;
        mediaController.controls.timeControl.loading = true;

        this._hasDeterminedMediaType = false;

        const media = mediaController.media;
        media.classList.add("media-document");
        media.classList.add("audio");

        let deviceType = window.navigator.platform;
        if (deviceType == "MacIntel")
            deviceType = GestureRecognizer.SupportsTouches ? "ipad" : "mac";

        media.classList.add(deviceType);

        media.addEventListener("error", this);
        media.addEventListener("play", this);
    }

    // Public

    layout()
    {
        if (!this._hasDeterminedMediaType)
            return;

        scheduler.scheduleLayout(() => {
            const media = this.mediaController.media;
            const isInvalid = media.error !== null && media.played.length === 0;
            const useVideoLayout = isInvalid || (media.readyState >= HTMLMediaElement.HAVE_METADATA && !this.mediaController.isAudio);

            const classList = media.classList;
            classList.toggle("invalid", isInvalid);
            classList.toggle("video", useVideoLayout);
            classList.toggle("audio", !useVideoLayout);
        });
    }

    // Protected

    handleEvent(event)
    {
        event.currentTarget.removeEventListener(event.type, this);

        if (event.type === "play" || event.type === "error") {
            this._hasDeterminedMediaType = true;
            this.layout();
        }
    }

}
