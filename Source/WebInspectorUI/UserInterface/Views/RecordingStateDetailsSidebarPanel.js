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
        this._index = NaN;

        this._dataGrid = null;
    }

    // Static

    static disallowInstanceForClass()
    {
        return true;
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
        this._index = NaN;

        for (let subview of this.contentView.subviews)
            this.contentView.removeSubview(subview);
    }

    updateActionIndex(index, context, options = {})
    {
        console.assert(!this._recording || (index >= 0 && index < this._recording.actions.length));
        if (!this._recording || index < 0 || index > this._recording.actions.length || index === this._index)
            return;

        this._index = index;

        if (this._recording.type === WI.Recording.Type.Canvas2D)
            this._generateDetailsCanvas2D(context, options);

        this.updateLayoutIfNeeded();
    }

    // Private

    _generateDetailsCanvas2D(context, options = {})
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

        let state = {
            direction: context.direction,
            fillStyle: context.fillStyle,
            font: context.font,
            globalAlpha: context.globalAlpha,
            globalCompositeOperation: context.globalCompositeOperation,
            imageSmoothingEnabled: context.imageSmoothingEnabled,
            imageSmoothingQuality: context.imageSmoothingQuality,
            lineCap: context.lineCap,
            lineDash: context.getLineDash(),
            lineDashOffset: context.lineDashOffset,
            lineJoin: context.lineJoin,
            lineWidth: context.lineWidth,
            miterLimit: context.miterLimit,
            shadowBlur: context.shadowBlur,
            shadowColor: context.shadowColor,
            shadowOffsetX: context.shadowOffsetX,
            shadowOffsetY: context.shadowOffsetY,
            strokeStyle: context.strokeStyle,
            textAlign: context.textAlign,
            textBaseline: context.textBaseline,
            transform: context.getTransform(),
            webkitImageSmoothingEnabled: context.webkitImageSmoothingEnabled,
            webkitLineDash: context.webkitLineDash,
            webkitLineDashOffset: context.webkitLineDashOffset,
        };

        let action = this._recording.actions[this._index];
        for (let name in state) {
            let value = state[name];
            if (typeof value === "object") {
                let isGradient = value instanceof CanvasGradient;
                let isPattern = value instanceof CanvasPattern;
                if (isGradient || isPattern) {
                    value = document.createElement("span");
                    value.classList.add("unavailable");
                    if (isGradient)
                        value.textContent = WI.unlocalizedString("Gradient");
                    else if (isPattern)
                        value.textContent = WI.unlocalizedString("Pattern");
                } else {
                    if (value instanceof DOMMatrix)
                        value = [value.a, value.b, value.c, value.d, value.e, value.f];

                    value = JSON.stringify(value);
                }
            }

            let classNames = [];
            if (!action.isGetter && action.stateModifiers.includes(name))
                classNames.push("modified");
            if (name.startsWith("webkit"))
                classNames.push("non-standard");

            const hasChildren = false;
            this._dataGrid.appendChild(new WI.DataGridNode({name, value}, hasChildren, classNames));
        }
    }
};
