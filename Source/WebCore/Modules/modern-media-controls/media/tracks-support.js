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

class TracksSupport extends MediaControllerSupport
{

    // Protected

    get control()
    {
        return this.mediaController.controls.tracksButton;
    }

    get mediaEvents()
    {
        return ["loadedmetadata"];
    }

    get tracksToMonitor()
    {
        return [this.mediaController.media.audioTracks, this.mediaController.media.textTracks];
    }

    buttonWasPressed(control)
    {
        this.mediaController.showMediaControlsContextMenu(control, {
            promoteSubMenus: this.mediaController.layoutTraits.promoteSubMenusWhenShowingMediaControlsContextMenu(),
        });
    }

    syncControl()
    {
        this.control.enabled = this.mediaController.canShowMediaControlsContextMenu && (this._canPickAudioTracks() || this._canPickTextTracks());
    }

    // Private

    _textTracks()
    {
        return this._sortedTrackList(this.mediaController.media.textTracks);
    }

    _audioTracks()
    {
        return this._sortedTrackList(this.mediaController.media.audioTracks);
    }

    _canPickAudioTracks()
    {
        const audioTracks = this._audioTracks();
        return audioTracks && audioTracks.length > 1;
    }

    _canPickTextTracks()
    {
        const textTracks = this._textTracks();
        return textTracks && textTracks.length > 0;
    }

    _sortedTrackList(tracks)
    {
        return this.mediaController.host ? this.mediaController.host.sortedTrackListForMenu(tracks) : Array.from(tracks);
    }

}
