/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

WebInspector.VisualStyleTimingEditor = class VisualStyleTimingEditor extends WebInspector.VisualStyleKeywordPicker
{
    constructor(propertyNames, text, possibleValues, layoutReversed)
    {
        super(propertyNames, text, possibleValues, layoutReversed);

        this.element.classList.add("timing-editor");

        this._customValueOptionElement = document.createElement("option");
        this._customValueOptionElement.value = "custom";
        this._customValueOptionElement.text = WebInspector.UIString("Custom");
        this._keywordSelectElement.appendChild(this._customValueOptionElement);

        this._bezierMarkerElement = document.createElement("span");
        this._bezierMarkerElement.title = WebInspector.UIString("Click to open a cubic-bezier editor");
        this._bezierMarkerElement.classList.add("bezier-editor");
        this._bezierMarkerElement.hidden = true;
        this._bezierMarkerElement.addEventListener("click", this._bezierMarkerClicked.bind(this));
        this.contentElement.appendChild(this._bezierMarkerElement);

        this._bezierEditor = new WebInspector.BezierEditor;
        this._bezierEditor.addEventListener(WebInspector.BezierEditor.Event.BezierChanged, this._valueDidChange.bind(this));
        this._bezierEditor.bezier = WebInspector.CubicBezier.fromString("linear");
    }

    // Protected

    parseValue(text)
    {
        return /(cubic-bezier\(.+\))/.exec(text);
    }

    get bezierValue()
    {
        var bezier = this._bezierEditor.bezier;
        if (!bezier)
            return null;

        return bezier.toString();
    }

    set bezierValue(text)
    {
        var bezier = WebInspector.CubicBezier.fromString(text);
        this._bezierEditor.bezier = bezier;
    }

    // Private

    _getValue()
    {
        return this._customValueOptionElement.selected ? this.bezierValue : super._getValue();
    }

    _setValue(value)
    {
        this.bezierValue = value;
        if (this.valueIsSupportedKeyword(value)) {
            super._setValue(value);
            this._bezierMarkerElement.hidden = true;
            return;
        }

        var bezier = this.bezierValue;
        this._customValueOptionElement.selected = !!bezier;
        this._bezierMarkerElement.hidden = !bezier;
        this.specialPropertyPlaceholderElement.hidden = !!bezier;
        if (!bezier)
            super._setValue(value);
    }

    _generateSynthesizedValue()
    {
        return this._customValueOptionElement.selected ? this.bezierValue : super._generateSynthesizedValue();
    }

    _bezierMarkerClicked()
    {
        var bounds = WebInspector.Rect.rectFromClientRect(this._bezierMarkerElement.getBoundingClientRect());
        this._cubicBezierEditorPopover = new WebInspector.Popover(this);
        this._cubicBezierEditorPopover.content = this._bezierEditor.element;
        this._cubicBezierEditorPopover.present(bounds.pad(2), [WebInspector.RectEdge.MIN_X]);
    }

    _handleKeywordChanged()
    {
        super._handleKeywordChanged();
        var customOptionSelected = this._customValueOptionElement.selected;
        this._bezierMarkerElement.hidden = !customOptionSelected;
        this.specialPropertyPlaceholderElement.hidden = !!customOptionSelected;
        if (customOptionSelected)
            this.bezierValue = "linear";
    }
};
