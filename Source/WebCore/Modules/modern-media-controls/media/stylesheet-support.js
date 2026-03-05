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

class StyleSheetSupport extends MediaControllerSupport
{

    constructor(mediaController)
    {
        super(mediaController);
        this._captionSheet = null;
        this._cachedCaptionCSS = "";
        this._trackSheets = [];
        this._cachedTrackCSS = [];
        this._syncAdoptedStyleSheets();
    }

    // Protected

    get control()
    {
        return null;
    }

    get mediaEvents()
    {
        return ["loadedmetadata"];
    }

    get tracksToMonitor()
    {
        return [this.mediaController.media.textTracks];
    }

    handleEvent(event)
    {
        this._syncAdoptedStyleSheets();
    }

    // Public

    captionStyleSheetsDidChange()
    {
        this._syncAdoptedStyleSheets();
    }

    // Private

    _syncAdoptedStyleSheets()
    {
        const host = this.mediaController.host;
        const shadowRoot = this.mediaController.shadowRoot;
        if (!host || !shadowRoot)
            return;

        const sheets = [...host.uaStyleSheets];

        const captionCSS = host.captionPreferencesStyleSheet || "";
        if (captionCSS !== this._cachedCaptionCSS) {
            if (captionCSS) {
                if (!this._captionSheet)
                    this._captionSheet = new CSSStyleSheet();
                this._captionSheet.replaceSync(captionCSS);
            } else
                this._captionSheet = null;
            this._cachedCaptionCSS = captionCSS;
        }
        if (this._captionSheet)
            sheets.push(this._captionSheet);

        const trackCSSList = host.showingTextTrackStyleSheets;
        for (let i = 0; i < trackCSSList.length; i++) {
            if (i >= this._trackSheets.length)
                this._trackSheets.push(new CSSStyleSheet());
            if (trackCSSList[i] !== this._cachedTrackCSS[i])
                this._trackSheets[i].replaceSync(trackCSSList[i]);
            sheets.push(this._trackSheets[i]);
        }
        this._trackSheets.length = trackCSSList.length;
        this._cachedTrackCSS = [...trackCSSList];

        shadowRoot.adoptedStyleSheets = sheets;
    }
}
