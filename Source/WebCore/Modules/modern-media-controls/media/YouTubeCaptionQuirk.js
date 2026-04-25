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
(function() {
    "use strict";

    class CaptionMirror {
        static _instances = new WeakMap();

        // Public methods:
        constructor(video) {
            // If there was a previous instance of CaptionMirror associated with the same video
            // invalidate the previous instance, which removes all event listeners and mutation
            // observers:
            let oldMirror = CaptionMirror._instances.get(video);
            if (oldMirror) {
                oldMirror.invalidate();
                CaptionMirror._instances.delete(video);
            }
            CaptionMirror._instances.set(video, this);

            this._video = video;
            this._player = video.parentElement.parentElement;
            this._captionWindowSelector = '.ytp-caption-window-container';
            this._mirrorTrack = video.addTextTrack('forced', 'YouTube Captions');
            this._mirrorTrack.mode = 'hidden';
            this._captionObserver = null;
            this._waitObserver = null;

            this._onCaptionsTrackListChanged = () => this._syncTrackList();
            this._onCaptionsChanged = () => this._syncTrackList();
            this._player.addEventListener('onCaptionsTrackListChanged', this._onCaptionsTrackListChanged);
            this._player.addEventListener('captionschanged', this._onCaptionsChanged);

            navigator.mediaSession.setActionHandler('togglecaptions', details => this._handleMediaSessionAction(details));
            navigator.mediaSession.setActionHandler('selectcaptiontrack', details => this._handleMediaSessionAction(details));

            this._onPresentationModeChanged = () => this._handlePresentationModeChanged();
            this._video.addEventListener('webkitpresentationmodechanged', this._onPresentationModeChanged);

            this._attachCaptionWindowObserver();

            this._syncTrackList();
        }

        invalidate() {
            if (this._captionObserver) {
                this._captionObserver.disconnect();
                this._captionObserver = null;
            }
            if (this._waitObserver) {
                this._waitObserver.disconnect();
                this._waitObserver = null;
            }
            this._mirrorTrack.mode = 'disabled';
            this._player.removeEventListener('onCaptionsTrackListChanged', this._onCaptionsTrackListChanged);
            this._player.removeEventListener('captionschanged', this._onCaptionsChanged);
            this._video.removeEventListener('webkitpresentationmodechanged', this._onPresentationModeChanged);
        }

        // Private methods:
        _syncTrackList() {
            var trackList = this._getTrackList();
            var activeTrack = this._getActiveTrack();
            navigator.mediaSession.captionTracks = trackList?.map(track => {
                return { label: track.displayName, language: track.languageCode, enabled: this._areTracksEqual(track, activeTrack) };
            }) ?? [];
            navigator.mediaSession.captionsEnabled = this._player.isSubtitlesOn();
        }

        _syncCues(captionWindowContainer) {
            while (this._mirrorTrack.cues?.length)
                this._mirrorTrack.removeCue(this._mirrorTrack.cues[0]);
            for (var captionWindow of captionWindowContainer.querySelectorAll('.caption-window')) {
                var text = Array.from(captionWindow.querySelectorAll('.captions-text > span')).map(span => span.textContent).join('\n');
                if (!text)
                    continue;
                var cue = new VTTCue(0, 10e7, text);
                switch (captionWindow.style.textAlign) {
                case 'left':
                case 'start':
                    cue.align = 'start';
                    cue.positionAlign = 'line-left'
                    break;
                case 'end':
                case 'right':
                    cue.align = 'end';
                    cue.positionAlign = 'line-right';
                    break;
                default:
                    cue.align = 'center';
                    cue.positionAlign = = 'center';
                }
                var topPercent = parseFloat(captionWindow.style.top);
                var bottomPercent = parseFloat(captionWindow.style.bottom);
                var leftPercent = parseFloat(captionWindow.style.left);
                var rightPercent = parseFloat(captionWindow.style.right);
                if (!isNaN(topPercent)) {
                    cue.snapToLines = false;
                    cue.line = topPercent;
                } else if (!isNaN(bottomPercent)) {
                    cue.snapToLines = false;
                    cue.line = 100 - bottomPercent;
                }
                if (!isNaN(leftPercent))
                    cue.position = leftPercent;
                else if (!isNaN(rightPercent))
                    cue.position = 100 - rightPercent;
                this._mirrorTrack.addCue(cue);
            }
        }

        _attachCaptionWindowObserver() {
            var container = this._player.querySelector(this._captionWindowSelector);
            if (container)
                this._attachCaptionObserver(container);
            else {
                this._waitObserver = new MutationObserver(() => {
                    var container = this._player.querySelector(this._captionWindowSelector);
                    if (container) {
                        this._attachCaptionObserver(container);
                        this._waitObserver.disconnect();
                        this._waitObserver = null;
                    }
                });
                this._waitObserver.observe(this._player, { childList: true, subtree: true });
            }
        }

        _attachCaptionObserver(container) {
            if (this._captionObserver)
                this._captionObserver.disconnect();
            this._captionObserver = new MutationObserver(() => {
                var container = this._player.querySelector(this._captionWindowSelector);
                if (container) this._syncCues(container);
            });
            this._captionObserver.observe(container, {
                childList: true,
                subtree: true,
                characterData: true,
            });
        }

        _handlePresentationModeChanged() {
            this._mirrorTrack.mode = this._video.webkitPresentationMode == 'inline' ? 'hidden' : 'showing';
        }

        _handleMediaSessionAction(details) {
            switch (details.action) {
            case 'togglecaptions':
                this._player.toggleSubtitles();
                break;
            case 'selectcaptiontrack': {
                var list = this._getTrackList();
                var index = details.trackIndex;
                if (index >= 0 && index < list.length)
                    this._player.setOption('captions', 'track', list[index]);
                else
                    this._player.setOption('captions', 'track', {});
                break;
            }
            }
        }

        // Utility methods:
        _getTrackList() { return this._player.getOption('captions', 'tracklist') || []; }
        _getActiveTrack() { return this._player.getOption('captions', 'track'); }
        _areTracksEqual(a, b) { return a && b && a.vss_id === b.vss_id;  }
    }

    window.setupCaptionMirroring = function(video) {
        new CaptionMirror(video);
    }
}());
