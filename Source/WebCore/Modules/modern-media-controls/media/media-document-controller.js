/*
 * Copyright (C) 2017 Apple Inc. All Rights Reserved.
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

class MediaDocumentController
{

    constructor(mediaController)
    {
        this.mediaController = mediaController;

        const media = mediaController.media;
        media.classList.add("media-document");

        if (media.readyState >= HTMLMediaElement.HAVE_METADATA)
            this._mediaDocumentHasMetadata();
        else
            media.addEventListener("loadedmetadata", this);
    }

    // Protected

    handleEvent(event)
    {
        event.currentTarget.removeEventListener(event.type, this);

        if (event.type === "loadedmetadata")
            this._mediaDocumentHasMetadata();
        else if (event.type === "resize")
            this._mediaDocumentHasSize();
    }

    // Private

    _mediaDocumentHasMetadata()
    {
        window.requestAnimationFrame(() => {
            const media = this.mediaController.media;
            media.classList.add(this.mediaController.isAudio ? "audio" : "video");
            media.classList.add(window.navigator.platform === "MacIntel" ? "mac" : window.navigator.platform);

            if (this.mediaController.isAudio)
                this._mediaDocumentHasSize();
            else
                this.mediaController.shadowRoot.addEventListener("resize", this);
        });
    }

    _mediaDocumentHasSize()
    {
        window.requestAnimationFrame(() => this.mediaController.media.classList.add("ready"));
    }

}
