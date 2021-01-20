/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

WI.CSSGrouping = class CSSGrouping
{
    constructor(type, text, sourceCodeLocation)
    {
        console.assert(Object.values(CSSGrouping.Type).includes(type));
        console.assert(typeof text === "string" && text.length);
        console.assert(!sourceCodeLocation || sourceCodeLocation instanceof WI.SourceCodeLocation);

        this._type = type;
        this._text = text;
        this._sourceCodeLocation = sourceCodeLocation || null;
    }

    // Public

    get type() { return this._type; }
    get text() { return this._text; }
    get sourceCodeLocation() { return this._sourceCodeLocation; }

    get isMedia()
    {
        return this._type === WI.CSSGrouping.Type.MediaRule
            || this._type === WI.CSSGrouping.Type.MediaImportRule
            || this._type === WI.CSSGrouping.Type.MediaLinkNode
            || this._type === WI.CSSGrouping.Type.MediaStyleNode;
    }

    get isSupports()
    {
        return this._type === WI.CSSGrouping.Type.SupportsRule;
    }

    get prefix()
    {
        if (this.isSupports)
            return "@supports";

        console.assert(this.isMedia);
        return "@media";
    }
};

WI.CSSGrouping.Type = {
    MediaRule: "media-rule",
    MediaImportRule: "media-import-rule",
    MediaLinkNode: "media-link-node",
    MediaStyleNode: "media-style-node",
    SupportsRule: "supports-rule",
};
