/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
 * Copyright (C) 2016 Devin Rousso <dcrousso+webkit@gmail.com>. All rights reserved.
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

WebInspector.InlineSwatch = class InlineSwatch extends WebInspector.Object
{
    constructor(type, value, readOnly)
    {
        super();

        this._type = type;

        this._swatchElement = document.createElement("span");
        this._swatchElement.classList.add("inline-swatch", this._type.split("-").lastValue);

        switch (this._type) {
        case WebInspector.InlineSwatch.Type.Bezier:
            this._swatchElement.title = WebInspector.UIString("Click to open a cubic-bezier editor.");
            break;
        case WebInspector.InlineSwatch.Type.Gradient:
            this._swatchElement.title = WebInspector.UIString("Click to select a gradient.");
            break;
        default:
            console.assert(this._type === WebInspector.InlineSwatch.Type.Color);
            this._swatchElement.title = WebInspector.UIString("Click to select a color. Shift-click to switch color formats.");
            break;
        }

        if (!readOnly) {
            this._swatchElement.addEventListener("click", this._swatchElementClicked.bind(this));
            if (this._type === WebInspector.InlineSwatch.Type.Color)
                this._swatchElement.addEventListener("contextmenu", this._handleContextMenuEvent.bind(this));
        }

        this._swatchInnerElement = this._swatchElement.createChild("span");

        this._value = value || this._fallbackValue();

        this._updateSwatch();
    }

    // Public

    get element()
    {
        return this._swatchElement;
    }

    get value()
    {
        return this._value;
    }

    set value(value)
    {
        this._value = value;
        this._updateSwatch(true);
    }

    // Private

    _fallbackValue()
    {
        switch (this._type) {
        case WebInspector.InlineSwatch.Type.Bezier:
            return WebInspector.CubicBezier.fromString("linear");
        case WebInspector.InlineSwatch.Type.Gradient:
            return WebInspector.Gradient.fromString("linear-gradient(transparent, transparent)");
        case WebInspector.InlineSwatch.Type.Color:
            return WebInspector.Color.fromString("transparent");
        default:
            return null;
        }
    }

    _updateSwatch(dontFireEvents)
    {
        if (this._type === WebInspector.InlineSwatch.Type.Color || this._type === WebInspector.InlineSwatch.Type.Gradient)
            this._swatchInnerElement.style.background = this._value ? this._value.toString() : null;

        if (!dontFireEvents)
            this.dispatchEventToListeners(WebInspector.InlineSwatch.Event.ValueChanged, {value: this._value});
    }

    _swatchElementClicked(event)
    {
        if (this._type === WebInspector.InlineSwatch.Type.Color && event.shiftKey && this._value) {
            let nextFormat = this._value.nextFormat();
            console.assert(nextFormat);
            if (!nextFormat)
                return;

            this._value.format = nextFormat;
            this._updateSwatch();
            return;
        }

        let bounds = WebInspector.Rect.rectFromClientRect(this._swatchElement.getBoundingClientRect());
        let popover = new WebInspector.Popover(this);

        let valueEditor = null;
        if (this._type === WebInspector.InlineSwatch.Type.Bezier) {
            valueEditor = new WebInspector.BezierEditor;
            valueEditor.addEventListener(WebInspector.BezierEditor.Event.BezierChanged, this._valueEditorValueDidChange, this);
        } else if (this._type === WebInspector.InlineSwatch.Type.Gradient) {
            valueEditor = new WebInspector.GradientEditor;
            valueEditor.addEventListener(WebInspector.GradientEditor.Event.GradientChanged, this._valueEditorValueDidChange, this);
            valueEditor.addEventListener(WebInspector.GradientEditor.Event.ColorPickerToggled, (event) => popover.update());
        } else {
            valueEditor = new WebInspector.ColorPicker;
            valueEditor.addEventListener(WebInspector.ColorPicker.Event.ColorChanged, this._valueEditorValueDidChange, this);
        }

        popover.content = valueEditor.element;
        popover.present(bounds.pad(2), [WebInspector.RectEdge.MIN_X]);

        let value = this._value || this._fallbackValue();
        if (this._type === WebInspector.InlineSwatch.Type.Bezier)
            valueEditor.bezier = value;
        else if (this._type === WebInspector.InlineSwatch.Type.Gradient)
            valueEditor.gradient = value;
        else
            valueEditor.color = value;
    }

    _valueEditorValueDidChange(event)
    {
        if (this._type === WebInspector.InlineSwatch.Type.Bezier)
            this._value = event.data.bezier;
        else if (this._type === WebInspector.InlineSwatch.Type.Gradient)
            this._value = event.data.gradient;
        else
            this._value = event.data.color;

        this._updateSwatch();
    }

    _handleContextMenuEvent(event)
    {
        let contextMenu = WebInspector.ContextMenu.createFromEvent(event);

        if (this._value.isKeyword() && this._value.format !== WebInspector.Color.Format.Keyword) {
            contextMenu.appendItem(WebInspector.UIString("Format: Keyword"), () => {
                this._value.format = WebInspector.Color.Format.Keyword;
                this._updateSwatch();
            });
        }

        let hexInfo = this._getNextValidHEXFormat();
        if (hexInfo) {
            contextMenu.appendItem(hexInfo.title, () => {
                this._value.format = hexInfo.format;
                this._updateSwatch();
            });
        }

        if (this._value.simple && this._value.format !== WebInspector.Color.Format.HSL) {
            contextMenu.appendItem(WebInspector.UIString("Format: HSL"), () => {
                this._value.format = WebInspector.Color.Format.HSL;
                this._updateSwatch();
            });
        } else if (this._value.format !== WebInspector.Color.Format.HSLA) {
            contextMenu.appendItem(WebInspector.UIString("Format: HSLA"), () => {
                this._value.format = WebInspector.Color.Format.HSLA;
                this._updateSwatch();
            });
        }

        if (this._value.simple && this._value.format !== WebInspector.Color.Format.RGB) {
            contextMenu.appendItem(WebInspector.UIString("Format: RGB"), () => {
                this._value.format = WebInspector.Color.Format.RGB;
                this._updateSwatch();
            });
        } else if (this._value.format !== WebInspector.Color.Format.RGBA) {
            contextMenu.appendItem(WebInspector.UIString("Format: RGBA"), () => {
                this._value.format = WebInspector.Color.Format.RGBA;
                this._updateSwatch();
            });
        }
    }

    _getNextValidHEXFormat()
    {
        if (this._type !== WebInspector.InlineSwatch.Type.Color)
            return false;

        function hexMatchesCurrentColor(hexInfo) {
            let nextIsSimple = hexInfo.format === WebInspector.Color.Format.ShortHEX || hexInfo.format === WebInspector.Color.Format.HEX;
            if (nextIsSimple && !this._value.simple)
                return false;

            let nextIsShort = hexInfo.format === WebInspector.Color.Format.ShortHEX || hexInfo.format === WebInspector.Color.Format.ShortHEXAlpha;
            if (nextIsShort && !this._value.canBeSerializedAsShortHEX())
                return false;

            return true;
        }

        const hexFormats = [
            {
                format: WebInspector.Color.Format.ShortHEX,
                title: WebInspector.UIString("Format: Short Hex")
            },
            {
                format: WebInspector.Color.Format.ShortHEXAlpha,
                title: WebInspector.UIString("Format: Short Hex with Alpha")
            },
            {
                format: WebInspector.Color.Format.HEX,
                title: WebInspector.UIString("Format: Hex")
            },
            {
                format: WebInspector.Color.Format.HEXAlpha,
                title: WebInspector.UIString("Format: Hex with Alpha")
            }
        ];

        // FIXME: <https://webkit.org/b/152497> Arrow functions: "this" isn't lexically bound
        let currentColorIsHEX = hexFormats.some(function(info) {
            return info.format === this._value.format;
        }.bind(this));

        for (let i = 0; i < hexFormats.length; ++i) {
            if (currentColorIsHEX && this._value.format !== hexFormats[i].format)
                continue;

            for (let j = ~~currentColorIsHEX; j < hexFormats.length; ++j) {
                let nextIndex = (i + j) % hexFormats.length;
                if (hexMatchesCurrentColor.call(this, hexFormats[nextIndex]))
                    return hexFormats[nextIndex];
            }
            return null;
        }
        return hexFormats[0];
    }
};

WebInspector.InlineSwatch.Type = {
    Color: "inline-swatch-type-color",
    Gradient: "inline-swatch-type-gradient",
    Bezier: "inline-swatch-type-bezier"
};

WebInspector.InlineSwatch.Event = {
    ValueChanged: "inline-swatch-value-changed"
};
