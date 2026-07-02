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

    // CNN's "Bolt" player renders captions into a React-managed overlay subtree
    // rather than via TextTracks on the <video>. As a result, when WebKit enters
    // its native fullscreen UI the captions disappear because they were never
    // exposed to the media element.
    //
    // The Bolt overlay is rooted at [data-testid="overlay-root"], and each
    // visible caption line is a descendant tagged [data-testid="cueBoxRowTextCue"].
    // We watch that subtree and mirror whatever text is currently rendered into a
    // hidden 'forced' TextTrack on the <video>. When the element enters fullscreen
    // (where Bolt's React overlay is no longer visible) we flip the track to
    // 'showing' so WebKit renders the captions itself.
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
            this._playerSelector = '[data-component-name="video-player"]';
            this._overlayRootSelector = '[data-testid="overlay-root"]';
            this._captionRowSelector = '[data-testid="cueBoxRowTextCue"]';
            this._player = this._findPlayerRoot(video);
            this._mirrorTrack = video.addTextTrack('forced', 'CNN Captions');
            this._mirrorTrack.mode = 'hidden';
            this._captionObserver = null;
            this._waitObserver = null;

            this._onPresentationModeChanged = () => this._handlePresentationModeChanged();
            this._video.addEventListener('webkitpresentationmodechanged', this._onPresentationModeChanged);

            if (this._player)
                this._attachOverlayObserver();
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
            this._video.removeEventListener('webkitpresentationmodechanged', this._onPresentationModeChanged);
        }

        // Private methods:
        _findPlayerRoot(video) {
            return video.closest(this._playerSelector);
        }

        _syncCues(overlayRoot) {
            while (this._mirrorTrack.cues?.length)
                this._mirrorTrack.removeCue(this._mirrorTrack.cues[0]);

            var rows = Array.from(overlayRoot.querySelectorAll(this._captionRowSelector));
            if (!rows.length)
                return;

            var text = rows.map(row => row.textContent).join('\n');
            if (!text)
                return;

            var cue = new VTTCue(0, 10e7, text);

            // Set some reasonable defaults for the cue display:
            cue.snapToLines = true;
            cue.line = -4;
            cue.position = 'auto';
            cue.positionAlign = 'center';

            this._mirrorTrack.addCue(cue);
        }

        // The overlay-root may not exist at the moment the <video> is connected — Bolt
        // mounts it asynchronously. If it isn't there yet, watch the player subtree until
        // it appears, then switch to the targeted caption observer.
        _attachOverlayObserver() {
            var overlayRoot = this._player.querySelector(this._overlayRootSelector);
            if (overlayRoot) {
                this._attachCaptionObserver(overlayRoot);
                return;
            }
            this._waitObserver = new MutationObserver(() => {
                var overlayRoot = this._player.querySelector(this._overlayRootSelector);
                if (overlayRoot) {
                    this._attachCaptionObserver(overlayRoot);
                    this._waitObserver.disconnect();
                    this._waitObserver = null;
                }
            });
            this._waitObserver.observe(this._player, { childList: true, subtree: true });
        }

        _attachCaptionObserver(overlayRoot) {
            if (this._captionObserver)
                this._captionObserver.disconnect();
            this._captionObserver = new MutationObserver(() => {
                // The overlay-root element itself may have been replaced by a React
                // remount; re-query rather than caching the original reference.
                var current = this._player.querySelector(this._overlayRootSelector);
                if (current)
                    this._syncCues(current);
            });
            this._captionObserver.observe(overlayRoot, {
                childList: true,
                subtree: true,
                characterData: true,
            });
            this._syncCues(overlayRoot);
        }

        _handlePresentationModeChanged() {
            if (this._video.webkitPresentationMode == 'inline') {
                this._mirrorTrack.mode = 'hidden';
                return;
            }
            this._mirrorTrack.mode = 'showing';
        }
    }

    window.setupCaptionMirroring = function(video) {
        new CaptionMirror(video);
    }
}());
