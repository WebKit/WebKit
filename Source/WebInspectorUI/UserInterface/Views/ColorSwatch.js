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

WebInspector.ColorSwatch = class ColorSwatch extends WebInspector.Object
{
    constructor(color, readOnly)
    {
        super();

        this._swatchElement = document.createElement("span");
        this._swatchElement.classList.add("color-swatch");
        this._swatchElement.title = WebInspector.UIString("Click to select a color. Shift-click to switch color formats.");
        if (!readOnly) {
            this._swatchElement.addEventListener("click", this._colorSwatchClicked.bind(this));
            this._swatchElement.addEventListener("contextmenu", this._handleContextMenuEvent.bind(this));
        }

        this._swatchInnerElement = document.createElement("span");
        this._swatchElement.appendChild(this._swatchInnerElement);

        this._color = color || WebInspector.Color.fromString("transparent");

        this._updateSwatch();
    }

    // Public

    get element()
    {
        return this._swatchElement;
    }

    set color(color)
    {
        this._color = color || WebInspector.Color.fromString("transparent");
        this._updateSwatch(true);
    }

    get color()
    {
        return this._color;
    }

    // Private

    _colorSwatchClicked(event)
    {
        if (event.shiftKey) {
            let nextFormat = this._color.nextFormat();
            console.assert(nextFormat);
            if (!nextFormat)
                return;

            this._color.format = nextFormat;
            this._updateSwatch();
            return;
        }

        let bounds = WebInspector.Rect.rectFromClientRect(this._swatchElement.getBoundingClientRect());

        let colorPicker = new WebInspector.ColorPicker;
        colorPicker.addEventListener(WebInspector.ColorPicker.Event.ColorChanged, this._colorPickerColorDidChange, this);

        let popover = new WebInspector.Popover(this);
        popover.content = colorPicker.element;
        popover.present(bounds.pad(2), [WebInspector.RectEdge.MIN_X]);

        colorPicker.color = this._color;
    }

    _colorPickerColorDidChange(event)
    {
        this._color = event.data.color;
        this._updateSwatch();
    }

    _handleContextMenuEvent(event)
    {
        let contextMenu = WebInspector.ContextMenu.createFromEvent(event);

        if (this._color.isKeyword() && this._color.format !== WebInspector.Color.Format.Keyword) {
            contextMenu.appendItem(WebInspector.UIString("Format: Keyword"), () => {
                this._color.format = WebInspector.Color.Format.Keyword;
                this._updateSwatch();
            });
        }

        let hexInfo = this._getNextValidHEXFormat();
        if (hexInfo) {
            contextMenu.appendItem(hexInfo.title, () => {
                this._color.format = hexInfo.format;
                this._updateSwatch();
            });
        }

        if (this._color.simple && this._color.format !== WebInspector.Color.Format.HSL) {
            contextMenu.appendItem(WebInspector.UIString("Format: HSL"), () => {
                this._color.format = WebInspector.Color.Format.HSL;
                this._updateSwatch();
            });
        } else if (this._color.format !== WebInspector.Color.Format.HSLA) {
            contextMenu.appendItem(WebInspector.UIString("Format: HSLA"), () => {
                this._color.format = WebInspector.Color.Format.HSLA;
                this._updateSwatch();
            });
        }

        if (this._color.simple && this._color.format !== WebInspector.Color.Format.RGB) {
            contextMenu.appendItem(WebInspector.UIString("Format: RGB"), () => {
                this._color.format = WebInspector.Color.Format.RGB;
                this._updateSwatch();
            });
        } else if (this._color.format !== WebInspector.Color.Format.RGBA) {
            contextMenu.appendItem(WebInspector.UIString("Format: RGBA"), () => {
                this._color.format = WebInspector.Color.Format.RGBA;
                this._updateSwatch();
            });
        }
    }

    _getNextValidHEXFormat()
    {
        function hexMatchesCurrentColor(hexInfo) {
            let nextIsSimple = hexInfo.format === WebInspector.Color.Format.ShortHEX || hexInfo.format === WebInspector.Color.Format.HEX;
            if (nextIsSimple && !this._color.simple)
                return false;

            let nextIsShort = hexInfo.format === WebInspector.Color.Format.ShortHEX || hexInfo.format === WebInspector.Color.Format.ShortHEXAlpha;
            if (nextIsShort && !this._color.canBeSerializedAsShortHEX())
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
            return info.format === this._color.format;
        }.bind(this));

        for (let i = 0; i < hexFormats.length; ++i) {
            if (currentColorIsHEX && this._color.format !== hexFormats[i].format)
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

    _updateSwatch(dontFireEvents)
    {
        this._swatchInnerElement.style.backgroundColor = this._color.toString() || null;
        if (!dontFireEvents)
            this.dispatchEventToListeners(WebInspector.ColorSwatch.Event.ColorChanged, {color: this._color});
    }
};

WebInspector.ColorSwatch.Event = {
    ColorChanged: "color-swatch-color-changed"
};
