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

class StartSupport extends MediaControllerSupport
{

    // Protected

    get control()
    {
        return this.mediaController.controls.startButton;
    }

    get mediaEvents()
    {
        return ["loadedmetadata", "play", "error", "webkitfullscreenchange"];
    }

    buttonWasPressed(control)
    {
        this.mediaController.media.play();
    }

    handleEvent(event)
    {
        if (event.type === "play")
            this._hasPlayed = true;

        super.handleEvent(event);
    }

    syncControl()
    {
        this.mediaController.controls.showsStartButton = this._shouldShowStartButton();
    }

    // Private

    _shouldShowStartButton()
    {
        const media = this.mediaController.media;

        if (this._hasPlayed || media.played.length)
            return false;

        if (!media.paused)
            return false;

        if (media.autoplay)
            return false;

        if (media instanceof HTMLAudioElement)
            return false;

        if (media.webkitDisplayingFullscreen)
            return false;

        if (!media.currentSrc)
            return false;

        if (media.error)
            return false;

        const host = this.mediaController.host;
        if (!media.controls && host && host.allowsInlineMediaPlayback)
            return false;

        return true;
    }

}
