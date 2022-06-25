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

class SeekSupport extends MediaControllerSupport
{

    // Protected

    get mediaEvents()
    {
        return ["durationchange"];
    }

    get multiplier()
    {
        // Implemented by subclasses.
    }

    buttonPressedStateDidChange(control, isPressed)
    {
        if (isPressed)
            this._startSeeking();
        else
            this._stopSeeking();
    }

    syncControl()
    {
        this.control.enabled = this.mediaController.media.duration !== Number.POSITIVE_INFINITY;
    }

    // Private

    _startSeeking()
    {
        const media = this.mediaController.media;
        const isPaused = media.paused;
        if (isPaused)
            media.play();

        this._wasPausedWhenSeekingStarted = isPaused;
        this._interval = window.setInterval(this._seek.bind(this), SeekSupport.SeekDelay);
        this._seek();
    }

    _stopSeeking()
    {
        const media = this.mediaController.media;
        media.playbackRate = media.defaultPlaybackRate;
        if (this._wasPausedWhenSeekingStarted)
            media.pause();
        if (this._interval)
            window.clearInterval(this._interval);
    }

    _seek()
    {
        const media = this.mediaController.media;
        media.playbackRate = Math.min(SeekSupport.MaximumSeekRate, Math.abs(media.playbackRate * 2)) * this.multiplier;
    }

}

SeekSupport.MaximumSeekRate = 8;
SeekSupport.SeekDelay = 1500;
