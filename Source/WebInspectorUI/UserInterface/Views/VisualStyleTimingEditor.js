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

WI.VisualStyleTimingEditor = class VisualStyleTimingEditor extends WI.VisualStyleKeywordPicker
{
    constructor(propertyNames, text, possibleValues, layoutReversed)
    {
        super(propertyNames, text, possibleValues, layoutReversed);

        this.element.classList.add("timing-editor");

        this._keywordSelectElement.createChild("hr");

        this._customBezierOptionElement = this._keywordSelectElement.createChild("option");
        this._customBezierOptionElement.value = "bezier";
        this._customBezierOptionElement.text = WI.UIString("Bezier");

        this._customSpringOptionElement = this._keywordSelectElement.createChild("option");
        this._customSpringOptionElement.value = "spring";
        this._customSpringOptionElement.text = WI.UIString("Spring");

        this._bezierSwatch = new WI.InlineSwatch(WI.InlineSwatch.Type.Bezier);
        this._bezierSwatch.addEventListener(WI.InlineSwatch.Event.ValueChanged, this._bezierSwatchValueChanged, this);
        this.contentElement.appendChild(this._bezierSwatch.element);

        this._springSwatch = new WI.InlineSwatch(WI.InlineSwatch.Type.Spring);
        this._springSwatch.addEventListener(WI.InlineSwatch.Event.ValueChanged, this._springSwatchValueChanged, this);
        this.contentElement.appendChild(this._springSwatch.element);
    }

    // Protected

    get value()
    {
        if (this._customBezierOptionElement.selected)
            return this._bezierValue;

        if (this._customSpringOptionElement.selected)
            return this._springValue;

        return super.value;
    }

    set value(value)
    {
        this._bezierValue = value;
        this._springValue = value;
        if (this.valueIsSupportedKeyword(value)) {
            super.value = value;
            this.contentElement.classList.remove("bezier-value", "spring-value");
            return;
        }

        let bezier = this._bezierValue;
        this._customBezierOptionElement.selected = !!bezier;
        this.contentElement.classList.toggle("bezier-value", !!bezier);

        let spring = this._springValue;
        this._customSpringOptionElement.selected = !!spring;
        this.contentElement.classList.toggle("spring-value", !!spring);

        this.specialPropertyPlaceholderElement.hidden = !!bezier || !!spring;
        if (!bezier && !spring)
            super.value = value;
    }

    get synthesizedValue()
    {
        if (this._customBezierOptionElement.selected)
            return this._bezierValue;

        if (this._customSpringOptionElement.selected)
            return this._springValue;

        return super.synthesizedValue;
    }

    parseValue(text)
    {
        return /((?:cubic-bezier|spring)\(.+\))/.exec(text);
    }

    // Private

    get _bezierValue()
    {
        let bezier = this._bezierSwatch.value;
        if (!bezier)
            return null;

        return bezier.toString();
    }

    set _bezierValue(text)
    {
        this._bezierSwatch.value = WI.CubicBezier.fromString(text);
    }

    get _springValue()
    {
        let spring = this._springSwatch.value;
        if (!spring)
            return null;

        return spring.toString();
    }

    set _springValue(text)
    {
        this._springSwatch.value = WI.Spring.fromString(text);
    }

    _handleKeywordChanged()
    {
        super._handleKeywordChanged();

        let customBezierOptionSelected = this._customBezierOptionElement.selected;
        this.contentElement.classList.toggle("bezier-value", !!customBezierOptionSelected);
        if (customBezierOptionSelected)
            this._bezierValue = "linear";

        let customSpringOptionSelected = this._customSpringOptionElement.selected;
        this.contentElement.classList.toggle("spring-value", !!customSpringOptionSelected);
        if (customSpringOptionSelected)
            this._springValue = "1 100 10 0";

        this.specialPropertyPlaceholderElement.hidden = !!customBezierOptionSelected || !!customSpringOptionSelected;
    }

    _bezierSwatchValueChanged(event)
    {
        this._valueDidChange();
    }

    _springSwatchValueChanged(event)
    {
        this._valueDidChange();
    }
};
