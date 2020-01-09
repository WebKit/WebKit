/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
 * Copyright (C) 2016 Devin Rousso <webkit@devinrousso.com>. All rights reserved.
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

WI.InlineSwatch = class InlineSwatch extends WI.Object
{
    constructor(type, value, readOnly)
    {
        super();

        this._type = type;

        if (this._type === WI.InlineSwatch.Type.Bezier || this._type === WI.InlineSwatch.Type.Spring)
            this._swatchElement = WI.ImageUtilities.useSVGSymbol("Images/CubicBezier.svg");
        else if (this._type === WI.InlineSwatch.Type.Variable)
            this._swatchElement = WI.ImageUtilities.useSVGSymbol("Images/CSSVariable.svg");
        else
            this._swatchElement = document.createElement("span");

        this._swatchElement.classList.add("inline-swatch", this._type.split("-").lastValue);

        if (readOnly)
            this._swatchElement.classList.add("read-only");
        else {
            switch (this._type) {
            case WI.InlineSwatch.Type.Color:
                // Handled later by _updateSwatch.
                break;
            case WI.InlineSwatch.Type.Gradient:
                this._swatchElement.title = WI.UIString("Edit custom gradient");
                break;
            case WI.InlineSwatch.Type.Bezier:
                this._swatchElement.title = WI.UIString("Edit \u201Ccubic-bezier\u201D function");
                break;
            case WI.InlineSwatch.Type.Spring:
                this._swatchElement.title = WI.UIString("Edit \u201Cspring\u201D function");
                break;
            case WI.InlineSwatch.Type.Variable:
                this._swatchElement.title = WI.UIString("Click to view variable value\nShift-click to replace variable with value");
                break;
            case WI.InlineSwatch.Type.Image:
                this._swatchElement.title = WI.UIString("View Image");
                break;
            default:
                WI.reportInternalError(`Unknown InlineSwatch type "${type}"`);
                break;
            }

            this._swatchElement.addEventListener("click", this._swatchElementClicked.bind(this));
            if (this._type === WI.InlineSwatch.Type.Color)
                this._swatchElement.addEventListener("contextmenu", this._handleContextMenuEvent.bind(this));
        }

        this._swatchInnerElement = this._swatchElement.createChild("span");

        this._value = value || this._fallbackValue();
        this._valueEditor = null;
        this._readOnly = readOnly;

        this._updateSwatch();
    }

    // Public

    get element()
    {
        return this._swatchElement;
    }

    get value()
    {
        if (typeof this._value === "function")
            return this._value();
        return this._value;
    }

    set value(value)
    {
        this._value = value;
        this._updateSwatch(true);
    }

    // Popover delegate

    didDismissPopover(popover)
    {
        if (!this._valueEditor)
            return;

        if (this._valueEditor.removeListeners)
            this._valueEditor.removeListeners();

        if (this._valueEditor instanceof WI.Object)
            this._valueEditor.removeEventListener(null, null, this);

        this._valueEditor = null;

        this.dispatchEventToListeners(WI.InlineSwatch.Event.Deactivated);
    }

    // Private

    _fallbackValue()
    {
        switch (this._type) {
        case WI.InlineSwatch.Type.Bezier:
            return WI.CubicBezier.fromString("linear");
        case WI.InlineSwatch.Type.Gradient:
            return WI.Gradient.fromString("linear-gradient(transparent, transparent)");
        case WI.InlineSwatch.Type.Spring:
            return WI.Spring.fromString("1 100 10 0");
        case WI.InlineSwatch.Type.Color:
            return WI.Color.fromString("white");
        default:
            return null;
        }
    }

    _updateSwatch(dontFireEvents)
    {
        let value = this.value;

        if (this._type === WI.InlineSwatch.Type.Color || this._type === WI.InlineSwatch.Type.Gradient)
            this._swatchInnerElement.style.background = value ? value.toString() : null;
        else if (this._type === WI.InlineSwatch.Type.Image)
            this._swatchInnerElement.style.setProperty("background-image", `url(${value.src})`);

        if (this._type === WI.InlineSwatch.Type.Color) {
            if (this._allowShiftClickColor())
                this._swatchElement.title = WI.UIString("Click to select a color\nShift-click to switch color formats");
            else
                this._swatchElement.title = WI.UIString("Click to select a color");
        }

        if (!dontFireEvents)
            this.dispatchEventToListeners(WI.InlineSwatch.Event.ValueChanged, {value});
    }

    _allowShiftClickColor()
    {
        return !this._readOnly && !this.value.isOutsideSRGB();
    }

    _swatchElementClicked(event)
    {
        event.stop();

        let value = this.value;

        if (event.shiftKey && value) {
            if (this._type === WI.InlineSwatch.Type.Color) {
                if (!this._allowShiftClickColor()) {
                    InspectorFrontendHost.beep();
                    return;
                }

                let nextFormat = value.nextFormat();
                console.assert(nextFormat);
                if (nextFormat) {
                    value.format = nextFormat;
                    this._updateSwatch();
                }
                return;
            }

            if (this._type === WI.InlineSwatch.Type.Variable) {
                // Force the swatch to replace the displayed text with the variable's value.
                this._swatchElement.remove();
                this._updateSwatch();
                return;
            }
        }

        if (this._valueEditor)
            return;

        if (!value)
            value = this._fallbackValue();

        let bounds = WI.Rect.rectFromClientRect(this._swatchElement.getBoundingClientRect());
        let popover = new WI.Popover(this);

        popover.windowResizeHandler = () => {
            let bounds = WI.Rect.rectFromClientRect(this._swatchElement.getBoundingClientRect());
            popover.present(bounds.pad(2), [WI.RectEdge.MIN_X]);
        };

        this._valueEditor = null;
        switch (this._type) {
        case WI.InlineSwatch.Type.Color:
            this._valueEditor = new WI.ColorPicker;
            this._valueEditor.addEventListener(WI.ColorPicker.Event.ColorChanged, this._valueEditorValueDidChange, this);
            break;

        case WI.InlineSwatch.Type.Gradient:
            this._valueEditor = new WI.GradientEditor;
            this._valueEditor.addEventListener(WI.GradientEditor.Event.GradientChanged, this._valueEditorValueDidChange, this);
            this._valueEditor.addEventListener(WI.GradientEditor.Event.ColorPickerToggled, (event) => popover.update());
            break;

        case WI.InlineSwatch.Type.Bezier:
            this._valueEditor = new WI.BezierEditor;
            this._valueEditor.addEventListener(WI.BezierEditor.Event.BezierChanged, this._valueEditorValueDidChange, this);
            break;

        case WI.InlineSwatch.Type.Spring:
            this._valueEditor = new WI.SpringEditor;
            this._valueEditor.addEventListener(WI.SpringEditor.Event.SpringChanged, this._valueEditorValueDidChange, this);
            break;

        case WI.InlineSwatch.Type.Variable:
            this._valueEditor = {};

            this._valueEditor.element = document.createElement("div");
            this._valueEditor.element.classList.add("inline-swatch-variable-popover");

            this._valueEditor.codeMirror = WI.CodeMirrorEditor.create(this._valueEditor.element, {
                mode: "css",
                readOnly: true,
            });
            this._valueEditor.codeMirror.on("update", () => {
                popover.update();
            });
            break;

        case WI.InlineSwatch.Type.Image:
            if (value.src) {
                this._valueEditor = {};
                this._valueEditor.element = document.createElement("img");
                this._valueEditor.element.src = value.src;
                this._valueEditor.element.classList.add("show-grid");
                this._valueEditor.element.style.setProperty("max-width", "50vw");
                this._valueEditor.element.style.setProperty("max-height", "50vh");
            }
            break;
        }

        if (!this._valueEditor)
            return;

        popover.content = this._valueEditor.element;
        popover.present(bounds.pad(2), [WI.RectEdge.MIN_X]);

        this.dispatchEventToListeners(WI.InlineSwatch.Event.Activated);

        switch (this._type) {
        case WI.InlineSwatch.Type.Color:
            this._valueEditor.color = value;
            this._valueEditor.focus();
            break;

        case WI.InlineSwatch.Type.Gradient:
            this._valueEditor.gradient = value;
            break;

        case WI.InlineSwatch.Type.Bezier:
            this._valueEditor.bezier = value;
            break;

        case WI.InlineSwatch.Type.Spring:
            this._valueEditor.spring = value;
            break;

        case WI.InlineSwatch.Type.Variable: {
            let codeMirror = this._valueEditor.codeMirror;
            codeMirror.setValue(value);

            const range = null;
            function optionsForType(type) {
                return {
                    allowedTokens: /\btag\b/,
                    callback(marker, valueObject, valueString) {
                        let swatch = new WI.InlineSwatch(type, valueObject, true);
                        codeMirror.setUniqueBookmark({line: 0, ch: 0}, swatch.element);
                    }
                };
            }
            createCodeMirrorColorTextMarkers(codeMirror, range, optionsForType(WI.InlineSwatch.Type.Color));
            createCodeMirrorGradientTextMarkers(codeMirror, range, optionsForType(WI.InlineSwatch.Type.Gradient));
            createCodeMirrorCubicBezierTextMarkers(codeMirror, range, optionsForType(WI.InlineSwatch.Type.Bezier));
            createCodeMirrorSpringTextMarkers(codeMirror, range, optionsForType(WI.InlineSwatch.Type.Spring));
            break;
        }
        }
    }

    _valueEditorValueDidChange(event)
    {
        if (this._type === WI.InlineSwatch.Type.Color)
            this._value = event.data.color;
        else if (this._type === WI.InlineSwatch.Type.Gradient)
            this._value = event.data.gradient;
        else if (this._type === WI.InlineSwatch.Type.Bezier)
            this._value = event.data.bezier;
        else if (this._type === WI.InlineSwatch.Type.Spring)
            this._value = event.data.spring;

        this._updateSwatch();
    }

    _handleContextMenuEvent(event)
    {
        let value = this.value;
        if (!value)
            return;

        let contextMenu = WI.ContextMenu.createFromEvent(event);
        let isColorOutsideSRGB = value.isOutsideSRGB();

        if (!isColorOutsideSRGB) {
            if (value.isKeyword() && value.format !== WI.Color.Format.Keyword) {
                contextMenu.appendItem(WI.UIString("Format: Keyword"), () => {
                    value.format = WI.Color.Format.Keyword;
                    this._updateSwatch();
                });
            }

            let hexInfo = this._getNextValidHEXFormat();
            if (hexInfo) {
                contextMenu.appendItem(hexInfo.title, () => {
                    value.format = hexInfo.format;
                    this._updateSwatch();
                });
            }

            if (value.simple && value.format !== WI.Color.Format.HSL) {
                contextMenu.appendItem(WI.UIString("Format: HSL"), () => {
                    value.format = WI.Color.Format.HSL;
                    this._updateSwatch();
                });
            } else if (value.format !== WI.Color.Format.HSLA) {
                contextMenu.appendItem(WI.UIString("Format: HSLA"), () => {
                    value.format = WI.Color.Format.HSLA;
                    this._updateSwatch();
                });
            }

            if (value.simple && value.format !== WI.Color.Format.RGB) {
                contextMenu.appendItem(WI.UIString("Format: RGB"), () => {
                    value.format = WI.Color.Format.RGB;
                    this._updateSwatch();
                });
            } else if (value.format !== WI.Color.Format.RGBA) {
                contextMenu.appendItem(WI.UIString("Format: RGBA"), () => {
                    value.format = WI.Color.Format.RGBA;
                    this._updateSwatch();
                });
            }

            if (value.format !== WI.Color.Format.ColorFunction) {
                contextMenu.appendItem(WI.UIString("Format: Color Function"), () => {
                    value.format = WI.Color.Format.ColorFunction;
                    this._updateSwatch();
                });
            }

            contextMenu.appendSeparator();
        }

        if (value.gamut !== WI.Color.Gamut.DisplayP3) {
            contextMenu.appendItem(WI.UIString("Convert to Display-P3"), () => {
                value.gamut = WI.Color.Gamut.DisplayP3;
                this._updateSwatch();
            });
        }

        if (value.gamut !== WI.Color.Gamut.SRGB) {
            let label = isColorOutsideSRGB ? WI.UIString("Clamp to sRGB") : WI.UIString("Convert to sRGB");
            contextMenu.appendItem(label, () => {
                value.gamut = WI.Color.Gamut.SRGB;
                this._updateSwatch();
            });
        }
    }

    _getNextValidHEXFormat()
    {
        if (this._type !== WI.InlineSwatch.Type.Color)
            return false;

        let value = this.value;

        function hexMatchesCurrentColor(hexInfo) {
            let nextIsSimple = hexInfo.format === WI.Color.Format.ShortHEX || hexInfo.format === WI.Color.Format.HEX;
            if (nextIsSimple && !value.simple)
                return false;

            let nextIsShort = hexInfo.format === WI.Color.Format.ShortHEX || hexInfo.format === WI.Color.Format.ShortHEXAlpha;
            if (nextIsShort && !value.canBeSerializedAsShortHEX())
                return false;

            return true;
        }

        const hexFormats = [
            {
                format: WI.Color.Format.ShortHEX,
                title: WI.UIString("Format: Short Hex")
            },
            {
                format: WI.Color.Format.ShortHEXAlpha,
                title: WI.UIString("Format: Short Hex with Alpha")
            },
            {
                format: WI.Color.Format.HEX,
                title: WI.UIString("Format: Hex")
            },
            {
                format: WI.Color.Format.HEXAlpha,
                title: WI.UIString("Format: Hex with Alpha")
            }
        ];

        let currentColorIsHEX = hexFormats.some((info) => info.format === value.format);

        for (let i = 0; i < hexFormats.length; ++i) {
            if (currentColorIsHEX && value.format !== hexFormats[i].format)
                continue;

            for (let j = ~~currentColorIsHEX; j < hexFormats.length; ++j) {
                let nextIndex = (i + j) % hexFormats.length;
                if (hexMatchesCurrentColor(hexFormats[nextIndex]))
                    return hexFormats[nextIndex];
            }
            return null;
        }
        return hexFormats[0];
    }
};

WI.InlineSwatch.Type = {
    Color: "inline-swatch-type-color",
    Gradient: "inline-swatch-type-gradient",
    Bezier: "inline-swatch-type-bezier",
    Spring: "inline-swatch-type-spring",
    Variable: "inline-swatch-type-variable",
    Image: "inline-swatch-type-image",
};

WI.InlineSwatch.Event = {
    ValueChanged: "inline-swatch-value-changed",
    Activated: "inline-swatch-activated",
    Deactivated: "inline-swatch-deactivated",
};
