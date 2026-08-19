/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

// Find in Video: YouTube caption fetch.
//
// YouTube doesn't expose its captions as <track> elements. They arrive as JSON3 "timedtext" responses
// on the player's own XHR. This runs in the page's main world so it can wrap XMLHttpRequest before
// the player issues those requests, then mirrors the captions into a hidden text track on the <video>
// so find-in-video can search them without affecting YouTube's own caption rendering.

(function() {
    "use strict";

    // Parse YouTube's JSON3 timedtext payload into [{ start, end, text }] in seconds.
    function parseCues(responseText)
    {
        let data;
        try {
            data = JSON.parse(responseText);
        } catch (e) {
            return null;
        }
        if (!data || !Array.isArray(data.events))
            return null;

        const cues = [];
        for (const event of data.events) {
            if (!event.segs)
                continue;
            const text = event.segs.map(segment => segment.utf8 || "").join("").trim();
            if (!text)
                continue;
            const start = event.tStartMs / 1000;
            const end = (event.tStartMs + (event.dDurationMs || 0)) / 1000;
            if (end > start)
                cues.push({ start, end, text });
        }
        return cues.length ? cues : null;
    }

    let injectedTrack = null;
    let trackVideo = null;

    function clearTrack()
    {
        while (injectedTrack && injectedTrack.cues && injectedTrack.cues.length)
            injectedTrack.removeCue(injectedTrack.cues[0]);
    }

    // The <video> currently on screen. The Shorts feed keeps several video elements alive and pauses the
    // off-screen ones, so the playing element is the active short. Fall back to the most viewport-visible
    // element when none is playing.
    function activeVideo()
    {
        const videos = Array.from(document.querySelectorAll("video")).filter(v => v.isConnected);
        if (videos.length <= 1)
            return videos[0] || null;
        const score = v => {
            const r = v.getBoundingClientRect();
            const w = Math.max(0, Math.min(r.right, window.innerWidth) - Math.max(r.left, 0));
            const h = Math.max(0, Math.min(r.bottom, window.innerHeight) - Math.max(r.top, 0));
            return (v.paused ? 0 : 1e12) + w * h;
        };
        return videos.reduce((best, v) => score(v) > score(best) ? v : best);
    }

    // Mirror the fetched cues onto a hidden track on the active player. If that element changed, empty
    // the old one's track (a script-added track can't be removed) and start a new one.
    function setCues(cues)
    {
        const video = activeVideo();
        if (!video)
            return;
        if (injectedTrack && trackVideo !== video) {
            clearTrack();
            injectedTrack = null;
        }
        trackVideo = video;
        if (!injectedTrack)
            injectedTrack = video.addTextTrack("captions", "WebKitFindInVideo");
        clearTrack();
        for (const cue of (cues || []))
            injectedTrack.addCue(new VTTCue(cue.start, cue.end, cue.text));
        injectedTrack.mode = "hidden";
    }

    // The video currently being watched: /watch?v=ID or /shorts/ID.
    function watchedVideoId()
    {
        const v = new URLSearchParams(location.search).get("v");
        if (v)
            return v;
        const match = location.pathname.match(/^\/shorts\/([^/?#]+)/);
        return match ? match[1] : null;
    }

    // Wrap the page's own XHR so we see the signed timedtext responses.
    const originalOpen = XMLHttpRequest.prototype.open;
    XMLHttpRequest.prototype.open = function(method, url) {
        if (typeof url === "string" && url.includes("/api/timedtext")) {
            this.addEventListener("load", () => {
                try {
                    if (this.status !== 200)
                        return;
                    const requestVideoId = new URL(url, location.href).searchParams.get("v");
                    const onShorts = location.pathname.startsWith("/shorts/");
                    if (!requestVideoId)
                        return;
                    // On Shorts the URL lags a swipe behind, but each fetch is for the short just opened, so
                    // trust it. On the watch page the URL is accurate, so still ignore hover-preview fetches.
                    if (!onShorts && requestVideoId !== watchedVideoId())
                        return;
                    let text = null;
                    const responseType = this.responseType;
                    if (responseType === "" || responseType === "text")
                        text = this.responseText;
                    else if (responseType === "arraybuffer" && this.response)
                        text = new TextDecoder().decode(this.response);
                    if (!text)
                        return;
                    const cues = parseCues(text);
                    if (cues)
                        setCues(cues);
                } catch (e) { }
            });
        }
        return originalOpen.apply(this, arguments);
    };

    // A new video starting on the tracked player shouldn't keep the previous one's cues.
    document.addEventListener("loadstart", event => {
        if (event.target === trackVideo)
            clearTrack();
    }, true);
})();
