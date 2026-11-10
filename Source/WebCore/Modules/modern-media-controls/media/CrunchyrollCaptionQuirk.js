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

    // Crunchyroll's player renders captions into a list layered over the <video>
    // rather than via TextTracks on the <video>. As a result, when WebKit shows the
    // video in picture-in-picture the captions disappear because they were never
    // exposed to the media element.
    //
    // The player mounts into #player-container. The child of #player-container that
    // contains the <video> is the media layer, and the caption renderer appends a
    // div > div > ul subtree to it, with one <li> per visible cue. None of those
    // elements have a class or id, so we find them by structure. We watch the media
    // layer and mirror whatever text is currently rendered into a hidden 'forced'
    // TextTrack on the <video>. When the element leaves the page we flip the track to
    // 'showing' so WebKit renders the captions itself.
    //
    // The player library takes over any text track that is added after it loads a
    // source, and keeps it 'hidden'. setupCaptionMirroring() is called as soon as the
    // <video> is connected, before the first load, so the track added here is left
    // alone. For the same reason, a <video> that is connected again keeps its existing
    // track rather than getting a new one.
    class CaptionMirror {
        static _instances = new WeakMap();
        static _tracks = new WeakMap();

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

            this._video = video;
            this._mediaLayer = this._findMediaLayer();
            if (!this._mediaLayer)
                return;
            CaptionMirror._instances.set(video, this);

            this._mirrorTrack = CaptionMirror._tracks.get(video);
            if (!this._mirrorTrack) {
                this._mirrorTrack = video.addTextTrack('forced', 'Crunchyroll Captions');
                CaptionMirror._tracks.set(video, this._mirrorTrack);
            }
            this._currentCue = null;
            this._currentText = '';

            this._onStateChanged = () => this._updateTrackMode();
            this._video.addEventListener('webkitpresentationmodechanged', this._onStateChanged);
            this._video.addEventListener('emptied', this._onStateChanged);

            this._captionObserver = new MutationObserver(() => this._syncCue());
            this._captionObserver.observe(this._mediaLayer, {
                childList: true,
                subtree: true,
                characterData: true,
            });

            this._updateTrackMode();
            this._syncCue();
        }

        invalidate() {
            this._captionObserver.disconnect();
            this._video.removeEventListener('webkitpresentationmodechanged', this._onStateChanged);
            this._video.removeEventListener('emptied', this._onStateChanged);
            this._setCueText('');
        }

        // Private methods:
        _findMediaLayer() {
            var playerContainer = this._video.closest('#player-container');
            if (!playerContainer)
                return null;
            return Array.from(playerContainer.children).find(child => child.contains(this._video)) ?? null;
        }

        _syncCue() {
            var cueList = this._mediaLayer.querySelector(':scope > div > div > ul');
            var text = cueList ? Array.from(cueList.children).map(item => item.innerText).filter(Boolean).join('\n') : '';
            this._setCueText(text);
        }

        _setCueText(text) {
            if (text === this._currentText)
                return;
            this._currentText = text;

            // TextTrack.cues is null while the track is disabled, so remove the cue we
            // added rather than walking the track's cue list.
            if (this._currentCue) {
                this._mirrorTrack.removeCue(this._currentCue);
                this._currentCue = null;
            }

            if (!text)
                return;

            this._currentCue = new VTTCue(0, 10e7, text);
            this._mirrorTrack.addCue(this._currentCue);
        }

        _getIsInline() {
            if (typeof this._video.webkitPresentationMode === 'undefined')
                return !this._video.webkitDisplayingFullscreen;
            return this._video.webkitPresentationMode == 'inline';
        }

        // Also called on 'emptied', in case the page changed the track's mode while
        // switching sources.
        _updateTrackMode() {
            var mode = this._getIsInline() ? 'hidden' : 'showing';
            if (this._mirrorTrack.mode != mode)
                this._mirrorTrack.mode = mode;
        }
    }

    window.setupCaptionMirroring = function(video) {
        new CaptionMirror(video);
    }
}());
