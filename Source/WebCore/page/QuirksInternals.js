/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

// @internal

async function installNetflixMediaSessionQuirk() {
    function sleepFor(duration) {
        return new Promise(resolve => { setTimeout(resolve, duration); });
    }
    function getNetflixAPI() {
        return window?.netflix?.appContext?.state?.playerApp?.getAPI();
    }
    function getWatchSessionId() {
        return getNetflixAPI()?.videoPlayer?.getAllPlayerSessionIds()?.find((val => val?.includes("watch")));
    }
    function getVideoId() {
        return getNetflixAPI()?.getVideoIdBySessionId(getWatchSessionId());
    }
    function getVideoPlayer() {
        return getNetflixAPI()?.videoPlayer?.getVideoPlayerBySessionId(getWatchSessionId());
    };

    while (!getVideoPlayer())
        await sleepFor(100);

    navigator.mediaSession.setActionHandler('play', event => {
        getVideoPlayer()?.play();
    });
    navigator.mediaSession.setActionHandler('pause', event => {
        getVideoPlayer()?.pause();
    });
    navigator.mediaSession.setActionHandler('seekto', event => {
        getVideoPlayer()?.seek(event.seekTime * 1000)
    });
    navigator.mediaSession.setActionHandler('seekforward', event => {
        let player = getVideoPlayer();
        if (!player)
            return;
        let targetTime = player.getCurrentTime() / 1000 + (event.seekOffset | 10);
        let duration = player.getDuration() / 1000;
        if (targetTime >= duration)
            targetTime = duration;
        player.seek(targetTime * 1000);
    });
    navigator.mediaSession.setActionHandler('seekbackward', event => {
        let player = getVideoPlayer();
        if (!player)
            return;
        let targetTime = player.getCurrentTime() / 1000 - (event.seekOffset | 10);
        if (targetTime <= 0)
            targetTime = 0;
        player.seek(targetTime * 1000);
    });

    function updatePlaybackState(event) {
        let player = getVideoPlayer();
        if (!player)
            return;
        navigator.mediaSession.setPositionState({
            duration: player.getDuration() / 1000,
            position: player.getCurrentTime() / 1000, playbackState: 1
        })
    };

    function updatePlayingState(event) {
        let player = getVideoPlayer();
        if (!player)
            return;
        navigator.mediaSession.playbackState = player.getPaused() ? 'paused' : 'playing';
    }

    let player = getVideoPlayer();
    player.addEventListener('durationchanged', updatePlaybackState);
    player.addEventListener('currenttimechanged', updatePlaybackState);
    player.addEventListener('pausedchanged', updatePlayingState);
    player.addEventListener('playingchanged', updatePlayingState);
}
