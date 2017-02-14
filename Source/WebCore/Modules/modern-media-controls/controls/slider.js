/*
 * Copyright (C) 2016 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

class Slider extends LayoutItem
{

    constructor({ layoutDelegate = null, cssClassName = "" } = {})
    {
        super({
            element: `<div class="slider ${cssClassName}"></div>`,
            layoutDelegate
        });

        this._canvas = new LayoutNode(`<canvas></canvas>`);

        this._input = new LayoutNode(`<input type="range" min="0" max="1" step="0.001" />`);
        this._input.element.addEventListener("change", this);
        this._input.element.addEventListener("input", this);

        this.value = 0;

        this.children = [this._canvas, this._input];
    }

    // Public

    get value()
    {
        if (this._value !== undefined)
            return this._value;
        return parseFloat(this._input.element.value);
    }

    set value(value)
    {
        if (this._valueIsChanging)
            return;

        this._value = value;
        this.markDirtyProperty("value");
        this.needsLayout = true;
    }

    get width()
    {
        return super.width;
    }

    set width(width)
    {
        super.width = width;
        this.needsLayout = true;
    }

    // Protected

    handleEvent(event)
    {
        switch (event.type) {
        case "input":
            this._handleInputEvent();
            break;
        case "change":
            this._handleChangeEvent();
            break;
        }
    }

    commitProperty(propertyName)
    {
        switch (propertyName) {
        case "value":
            this._input.element.value = this._value;
            delete this._value;
            break;
        case "width":
            this._canvas.element.width = this.width * window.devicePixelRatio;
        case "height":
            this._canvas.element.height = this.height * window.devicePixelRatio;
        default :
            super.commitProperty(propertyName);
            break;
        }
    }

    layout()
    {
        super.layout();
        this.draw(this._canvas.element.getContext("2d"));
    }

    draw(ctx)
    {
        // Implemented by subclasses.
    }

    // Private

    _handleInputEvent()
    {
        if (!this._valueIsChanging && this.uiDelegate && typeof this.uiDelegate.controlValueWillStartChanging === "function")
            this.uiDelegate.controlValueWillStartChanging(this);
        this._valueIsChanging = true;
        if (this.uiDelegate && typeof this.uiDelegate.controlValueDidChange === "function")
            this.uiDelegate.controlValueDidChange(this);

        this.needsLayout = true;
    }

    _handleChangeEvent()
    {
        delete this._valueIsChanging;
        if (this.uiDelegate && typeof this.uiDelegate.controlValueDidStopChanging === "function")
            this.uiDelegate.controlValueDidStopChanging(this);

        this.needsLayout = true;
    }

}

function addRoundedRect(ctx, x, y, width, height, radius) {
    ctx.moveTo(x + radius, y);
    ctx.arcTo(x + width, y, x + width, y + radius, radius);
    ctx.lineTo(x + width, y + height - radius);
    ctx.arcTo(x + width, y + height, x + width - radius, y + height, radius);
    ctx.lineTo(x + radius, y + height);
    ctx.arcTo(x, y + height, x, y + height - radius, radius);
    ctx.lineTo(x, y + radius);
    ctx.arcTo(x, y, x + radius, y, radius);
}
