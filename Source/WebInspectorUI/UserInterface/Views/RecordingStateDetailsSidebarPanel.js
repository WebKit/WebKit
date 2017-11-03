/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

WI.RecordingStateDetailsSidebarPanel = class RecordingStateDetailsSidebarPanel extends WI.DetailsSidebarPanel
{
    constructor()
    {
        super("recording-state", WI.UIString("State"));

        this._recording = null;
        this._action = null;

        this._dataGrid = null;
    }

    // Public

    inspect(objects)
    {
        if (!(objects instanceof Array))
            objects = [objects];

        this.recording = objects.find((object) => object instanceof WI.Recording && object.type === WI.Recording.Type.Canvas2D);

        return !!this._recording;
    }

    set recording(recording)
    {
        if (recording === this._recording)
            return;

        this._recording = recording;
        this._action = null;

        for (let subview of this.contentView.subviews)
            this.contentView.removeSubview(subview);
    }

    updateAction(action, context, options = {})
    {
        if (!this._recording || action === this._action)
            return;

        this._action = action;

        if (this._recording.type === WI.Recording.Type.Canvas2D)
            this._generateDetailsCanvas2D(action, context, options);

        this.updateLayoutIfNeeded();
    }

    // Private

    _generateDetailsCanvas2D(action, context, options = {})
    {
        if (!this._dataGrid) {
            this._dataGrid = new WI.DataGrid({
                name: {title: WI.UIString("Name")},
                value: {title: WI.UIString("Value")},
            });
        }
        if (!this._dataGrid.parentView)
            this.contentView.addSubview(this._dataGrid);

        this._dataGrid.removeChildren();

        if (!context)
            return;

        let state = {};

        if (WI.RecordingContentView.supportsCanvasPathDebugging()) {
            state.currentX = context.currentX;
            state.currentY = context.currentY;
        }

        state.direction = context.direction;
        state.fillStyle = context.fillStyle;
        state.font = context.font;
        state.globalAlpha = context.globalAlpha;
        state.globalCompositeOperation = context.globalCompositeOperation;
        state.imageSmoothingEnabled = context.imageSmoothingEnabled;
        state.imageSmoothingQuality = context.imageSmoothingQuality;
        state.lineCap = context.lineCap;
        state.lineDash = context.getLineDash();
        state.lineDashOffset = context.lineDashOffset;
        state.lineJoin = context.lineJoin;
        state.lineWidth = context.lineWidth;
        state.miterLimit = context.miterLimit;
        state.shadowBlur = context.shadowBlur;
        state.shadowColor = context.shadowColor;
        state.shadowOffsetX = context.shadowOffsetX;
        state.shadowOffsetY = context.shadowOffsetY;
        state.strokeStyle = context.strokeStyle;
        state.textAlign = context.textAlign;
        state.textBaseline = context.textBaseline;
        state.transform = context.getTransform();
        state.webkitImageSmoothingEnabled = context.webkitImageSmoothingEnabled;
        state.webkitLineDash = context.webkitLineDash;
        state.webkitLineDashOffset = context.webkitLineDashOffset;

        function isColorProperty(name) {
            return name === "fillStyle" || name === "strokeStyle" || name === "shadowColor";
        }

        function createInlineSwatch(value) {
            let color = WI.Color.fromString(value);
            if (!color)
                return null;

            const readOnly = true;
            return new WI.InlineSwatch(WI.InlineSwatch.Type.Color, color, readOnly);
        }

        for (let name in state) {
            let value = state[name];
            if (typeof value === "object") {
                let isGradient = value instanceof CanvasGradient;
                let isPattern = value instanceof CanvasPattern;
                if (isGradient || isPattern) {
                    let textElement = document.createElement("span");
                    textElement.classList.add("unavailable");

                    let image = null;
                    if (isGradient) {
                        textElement.textContent = WI.unlocalizedString("CanvasGradient");
                        image = WI.ImageUtilities.imageFromCanvasGradient(value, 100, 100);
                    } else if (isPattern) {
                        textElement.textContent = WI.unlocalizedString("CanvasPattern");
                        image = value.__image;
                    }

                    let fragment = document.createDocumentFragment();
                    if (image) {
                        let swatch = new WI.InlineSwatch(WI.InlineSwatch.Type.Image, image);
                        fragment.appendChild(swatch.element);
                    }
                    fragment.appendChild(textElement);
                    value = fragment;
                } else {
                    if (value instanceof DOMMatrix)
                        value = [value.a, value.b, value.c, value.d, value.e, value.f];

                    value = JSON.stringify(value);
                }
            } else if (isColorProperty(name)) {
                let swatch = createInlineSwatch(value);
                let label = swatch.value.toString();
                value = document.createElement("span");
                value.append(swatch.element, label);
            }

            let classNames = [];
            if (!action.isGetter && action.stateModifiers.has(name))
                classNames.push("modified");
            if (name.startsWith("webkit"))
                classNames.push("non-standard");

            const hasChildren = false;
            this._dataGrid.appendChild(new WI.DataGridNode({name, value}, hasChildren, classNames));
        }
    }
};
