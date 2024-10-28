/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

class MetadataContainer extends LayoutNode
{
    constructor({ cssClassName } = {})
    {
        super(`<div class="metadata-container ${cssClassName ?? ""}"></div>`);
        
        this._title = "";
        this._artist = "";
        
        this._titleNode = new LayoutNode(`<div class="title-label"></div>`);
        this._artistNode = new LayoutNode(`<div class="artist-label"></div>`);
        this.children = [this._titleNode, this._artistNode];
    }

    // Public

    get title()
    {
        return this._title;
    }

    set title(title)
    {
        if (title === this._title)
            return;

        this._title = title;
        this.markDirtyProperty("title");
    }

    get artist()
    {
        return this._artist;
    }

    set artist(artist)
    {
        if (artist === this._artist)
            return;

        this._artist = artist;
        this.markDirtyProperty("artist");
    }

    // Protected
    
    commitProperty(propertyName)
    {
        if (propertyName === "title") {
            this._titleNode.element.textContent = this.title;
            this._titleNode.element.setAttribute("aria-label", UIString("Title: %s", this.title));
        } else if (propertyName === "artist") {
            this._artistNode.element.textContent = this.artist;
            this._artistNode.element.setAttribute("aria-label", UIString("Artist: %s", this.artist));
        } else
            super.commitProperty(propertyName)
    }
}
