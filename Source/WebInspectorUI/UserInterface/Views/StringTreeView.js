/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

WI.StringTreeView = class StringTreeView extends WI.Object
{
    constructor(string)
    {
        super();
        console.assert(typeof string === "string");
        this._string = string;
        this._currentIndex = 0;
        this._element = document.createElement("span");
        this._element.className = "string-tree formatted-string";
        this._stringContainer = this._element.appendChild(document.createElement("span"));
        this._stringContainer.className = "string-content";
        this._stringContainer.append("\"");
        this._chunksContainer = this._stringContainer.appendChild(document.createElement("span"));
        this._addNextChunk();
    }

    static get chunkSize() { return 15000; }

    get element()
    {
        return this._element;
    }

    _addNextChunk()
    {
        if (this._showMoreLink) {
            this._showMoreLink.remove();
            this._showMoreLink = null;
        }

        if (this._closingQuoteElement) {
            this._closingQuoteElement.remove();
            this._closingQuoteElement = null;
        }

        let chunkEnd = this._currentIndex + WI.StringTreeView.chunkSize;
        let hasMore = chunkEnd < this._string.length;

        if (!hasMore)
            chunkEnd = this._string.length;

        let chunk = this._string.slice(this._currentIndex, chunkEnd);
        let escapedChunk = chunk.replace(/\\/g, "\\\\").replace(/"/g, "\\\"").replace(/\n/g, "\u21B5");
        let linkifiedChunk = WI.linkifyStringAsFragment(escapedChunk);
        this._chunksContainer.appendChild(linkifiedChunk);
        this._currentIndex = chunkEnd;

        if (hasMore) {
            this._showMoreLink = document.createElement("span");
            this._showMoreLink.className = "string-show-more-link";
            this._showMoreLink.textContent = "(show more)";
            this._showMoreLink.addEventListener("click", (event) => {
                event.stop();
                this._addNextChunk();
            });
            this._chunksContainer.appendChild(this._showMoreLink);
        }

        this._closingQuoteElement = this._stringContainer.appendChild(document.createTextNode("\""));
        this.dispatchEventToListeners(WI.StringTreeView.Event.Updated);
    }
};

WI.StringTreeView.Event = {
    Updated: "string-tree-view-updated"
};
