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

// Find in Video: Netflix caption fetch.
//
// Netflix delivers subtitles as TTML over XHR/fetch and caches each language (no request on a
// switch-back), so we cache the parsed cues per language and follow the active language via Netflix's
// player API, mirroring it into a single hidden text track for find-in-video.

(function() {
    "use strict";

    // Parse a TTML document into [{ start, end, text }] in seconds, or null if it isn't TTML.
    function parseCues(text)
    {
        if (!text || text.indexOf("<tt") === -1)
            return null;

        let doc;
        try {
            doc = new DOMParser().parseFromString(text, "application/xml");
        } catch (e) {
            return null;
        }
        if (!doc || doc.getElementsByTagName("parsererror").length)
            return null;

        const tt = doc.documentElement;
        if (!tt)
            return null;

        const language = tt.getAttribute("xml:lang")
            || tt.getAttributeNS("http://www.w3.org/XML/1998/namespace", "lang")
            || "";

        // TTML times may be tick-based, so read the tick rate if the document declares one.
        const tickRate = parseFloat(tt.getAttribute("ttp:tickRate")
            || tt.getAttributeNS("http://www.w3.org/ns/ttml#parameter", "tickRate")
            || "0");

        function toSeconds(value)
        {
            if (!value)
                return NaN;
            value = value.trim();
            const last = value.charAt(value.length - 1);
            if (last === "t")
                return tickRate > 0 ? parseFloat(value) / tickRate : NaN;
            if (last === "s")
                return parseFloat(value);
            const parts = value.split(":");
            if (parts.length === 3)
                return (+parts[0]) * 3600 + (+parts[1]) * 60 + parseFloat(parts[2]);
            return parseFloat(value);
        }

        // Merge <p>s that share the same time range into a single cue.
        const paragraphs = tt.getElementsByTagNameNS("*", "p");
        const byRange = new Map();
        for (let i = 0; i < paragraphs.length; ++i) {
            const paragraph = paragraphs[i];
            const start = toSeconds(paragraph.getAttribute("begin"));
            const end = toSeconds(paragraph.getAttribute("end"));
            if (!(end > start))
                continue;
            const text = (paragraph.textContent || "").replace(/\s+/g, " ").trim();
            if (!text)
                continue;
            const key = start + "-" + end;
            const existing = byRange.get(key);
            if (existing)
                existing.text += " " + text;
            else
                byRange.set(key, { start, end, text });
        }

        const cues = Array.from(byRange.values());
        return cues.length ? { language, cues } : null;
    }

    const cueCache = new Map();
    let boundVideo = null;
    let injectedTrack = null;
    let appliedCues = null;
    let appliedVideoId = null;
    let listening = false;
    let pendingResults = [];

    // Resolve the live player element, re-acquiring it if Netflix tore down the one we had.
    function currentVideo()
    {
        if (boundVideo && boundVideo.isConnected)
            return boundVideo;
        const video = document.querySelector("video");
        return video && video.isConnected ? video : null;
    }

    function setCues(cues)
    {
        const video = currentVideo();
        if (!video)
            return;
        if (video !== boundVideo) {
            boundVideo = video;
            injectedTrack = null;
        }
        if (!injectedTrack)
            injectedTrack = boundVideo.addTextTrack("captions", "WebKitFindInVideo");
        clearTrack();
        for (const cue of (cues || []))
            injectedTrack.addCue(new VTTCue(cue.start, cue.end, cue.text));
        injectedTrack.mode = "hidden";
    }

    window.__setupFindInVideoCaptionFetch = function(video) {
        if (boundVideo === video)
            return;
        boundVideo = video;
        injectedTrack = null;
        // New element with an empty track: forget applied state and rebuild from the cache.
        appliedCues = null;
        applyActiveLanguage();
    };

    // The current playback via Netflix's player API: { videoId, language } where language is a bcp47
    // code, "OFF" when disabled, or null when transitioning. null overall if the API isn't ready.
    function session()
    {
        try {
            const api = netflix.appContext.state.playerApp.getAPI();
            const player = api.videoPlayer;
            const sessionId = player.getAllPlayerSessionIds()[0];
            if (!sessionId)
                return null;
            const track = player.getCurrentTextTrackBySessionId(sessionId);
            let videoId = "";
            try {
                videoId = api.getVideoIdBySessionId(sessionId) || "";
            } catch (e) { }
            // Without a videoId the cache key would collapse across episodes, so treat it as not ready.
            if (!videoId)
                return null;
            return {
                videoId,
                language: !track ? null : (track.isNoneTrack ? "OFF" : track.bcp47)
            };
        } catch (e) {
            return null;
        }
    }

    function clearTrack()
    {
        while (injectedTrack && injectedTrack.cues && injectedTrack.cues.length)
            injectedTrack.removeCue(injectedTrack.cues[0]);
    }

    // Mirror the active (videoId, language) transcript from the cache onto the track. On an episode
    // change, evict other episodes' cues. Re-apply when the cues change or Netflix tore down our element.
    function applyActiveLanguage()
    {
        const s = session();
        if (!s)
            return;
        if (s.videoId !== appliedVideoId) {
            appliedVideoId = s.videoId;
            appliedCues = null;
            clearTrack();
            for (const key of cueCache.keys())
                if (!key.startsWith(s.videoId + "\n"))
                    cueCache.delete(key);
        }
        if (!s.language || s.language === "OFF")
            return;
        const trackIsLive = injectedTrack && boundVideo && boundVideo.isConnected;
        const cues = cueCache.get(s.videoId + "\n" + s.language);
        if (!cues || (cues === appliedCues && trackIsLive))
            return;
        setCues(cues);
        appliedCues = cues;
    }

    // Drain transcripts that arrived before the player API was ready, then sync the active language.
    function onPlayerStateChanged()
    {
        if (pendingResults.length) {
            const results = pendingResults;
            pendingResults = [];
            for (const result of results)
                cacheResult(result);
        }
        applyActiveLanguage();
    }

    // Netflix has no "captions changed" event, but its Redux store fires on the change (even while
    // paused). Subscribe once, lazily, since the API isn't ready at document-start.
    function ensureListening()
    {
        if (listening)
            return;
        try {
            // Registering is the side effect we want, the return value varies, so treat a non-throwing call as subscribed.
            netflix.appContext.state.playerApp.addChangeListener(onPlayerStateChanged);
            listening = true;
        } catch (e) { }
        if (!listening) {
            document.addEventListener("timeupdate", onPlayerStateChanged, true);
            listening = true;
        }
    }

    // Cache a fetched transcript under (videoId, active language), falling back to the TTML's own
    // language for the language part, then apply it if it's the on-screen episode and language.
    function cacheResult(result)
    {
        const s = session();
        if (!s) {
            // The transcript arrived before the player API was ready, retry once it is.
            pendingResults.push(result);
            ensureListening();
            return;
        }
        const language = (s.language && s.language !== "OFF") ? s.language : result.language;
        if (!language)
            return;
        cueCache.set(s.videoId + "\n" + language, result.cues);
        ensureListening();
        applyActiveLanguage();
    }

    function isXML(contentType)
    {
        return contentType && contentType.indexOf("xml") !== -1;
    }

    // Wrap the page's own XHR so we see the TTML responses. Hook each request object at most once,
    // since Netflix reuses XHRs and re-opens them, which would otherwise stack a listener per open.
    const hookedRequests = new WeakSet();
    const originalOpen = XMLHttpRequest.prototype.open;
    XMLHttpRequest.prototype.open = function() {
        if (!hookedRequests.has(this)) {
            hookedRequests.add(this);
            this.addEventListener("load", function() {
                try {
                    if (!isXML(this.getResponseHeader("content-type")))
                        return;
                    let text = null;
                    const responseType = this.responseType;
                    if (responseType === "" || responseType === "text")
                        text = this.responseText;
                    else if (responseType === "arraybuffer" && this.response)
                        text = new TextDecoder().decode(this.response);
                    const result = parseCues(text);
                    if (result)
                        cacheResult(result);
                } catch (e) { }
            });
        }
        return originalOpen.apply(this, arguments);
    };

    // Netflix also fetches TTML via fetch(), so observe those responses too.
    const originalFetch = window.fetch;
    if (originalFetch) {
        window.fetch = function() {
            return originalFetch.apply(this, arguments).then(function(response) {
                try {
                    if (response && isXML(response.headers.get("content-type"))) {
                        response.clone().text().then(function(text) {
                            const result = parseCues(text);
                            if (result)
                                cacheResult(result);
                        }).catch(function() { });
                    }
                } catch (e) { }
                return response;
            });
        };
    }
})();
