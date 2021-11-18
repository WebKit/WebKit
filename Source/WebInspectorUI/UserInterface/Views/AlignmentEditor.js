/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

WI.AlignmentEditor = class AlignmentEditor extends WI.Object
{
    constructor()
    {
        super();

        this._value = null;
        this._valueToGlyphElement = new Map;

        this._element = document.createElement("div");
        this._element.className = "alignment-editor";

        // FIXME: <https://webkit.org/b/233053> Web Inspector: mirror/rotate alignment icons when flex-direction/grid-auto-flow/RTL affect axis or direction
        for (let [value, path] of Object.entries(WI.AlignmentEditor.ValueGlyphs)) {
            let glyphElement = WI.ImageUtilities.useSVGSymbol(path, "glyph", value);
            this._element.append(glyphElement);
            glyphElement.addEventListener("click", () => {
                this.dispatchEventToListeners(WI.AlignmentEditor.Event.ValueChanged, {value});
                this.value = value;
            });
            this._valueToGlyphElement.set(value, glyphElement);
        }
        this._updateSelected();
    }

    // Static

    static isAlignContentValue(value)
    {
        return WI.AlignmentEditor.ValueGlyphs.hasOwnProperty(value);
    }

    // Public

    get element() { return this._element; }

    get value()
    {
        return this._value;
    }

    set value(value)
    {
        if (this._value && WI.AlignmentEditor.isAlignContentValue(this._value)) {
            let previousGlyphElement = this._valueToGlyphElement.get(this._value);
            previousGlyphElement.classList.remove("selected");
        }
        this._value = value;
        this._updateSelected();
    }

    // Private

    _updateSelected()
    {
        if (!this._value || !WI.AlignmentEditor.isAlignContentValue(this._value))
            return;

        let glyphElement = this._valueToGlyphElement.get(this._value);
        glyphElement.classList.add("selected");
    }
};

// FIXME: <https://webkit.org/b/233054> Web Inspector: Add a swatch for align-items and align-self
// FIXME: <https://webkit.org/b/233055> Web Inspector: Add a swatch for justify-content, justify-items, and justify-self
WI.AlignmentEditor.ValueGlyphs = {
    "start": "Images/AlignmentStart.svg",
    "center": "Images/AlignmentCenter.svg",
    "end": "Images/AlignmentEnd.svg",
    "space-between": "Images/AlignmentSpaceBetween.svg",
    "space-around": "Images/AlignmentSpaceAround.svg",
    "space-evenly": "Images/AlignmentSpaceEvenly.svg",
    "stretch": "Images/AlignmentStretch.svg",
};

WI.AlignmentEditor.UnknownValueGlyph = "Images/AlignmentUnknown.svg";

WI.AlignmentEditor.Event = {
    ValueChanged: "alignment-editor-value-changed",
};
