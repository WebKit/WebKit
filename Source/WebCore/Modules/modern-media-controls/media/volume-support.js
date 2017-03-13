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

class VolumeSupport extends MediaControllerSupport
{

    // Protected

    get control()
    {
        return this.mediaController.controls.volumeSlider;
    }

    get mediaEvents()
    {
        return ["volumechange"];
    }

    controlValueWillStartChanging(control)
    {
        this._volumeBeforeChange = this.mediaController.media.volume;
        this.mediaController.media.muted = false;
    }

    controlValueDidChange(control)
    {
        this.mediaController.media.volume = (control.value === 0 && this._volumeBeforeChange > 0) ? this._volumeBeforeChange : control.value;
        this.mediaController.media.muted = control.value === 0;
    }

    syncControl()
    {
        const media = this.mediaController.media;
        this.control.value = media.muted ? 0 : media.volume;
    }

}
