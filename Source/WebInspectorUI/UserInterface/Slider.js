/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

WebInspector.Slider = function()
{
    this._element = document.createElement("div");
    this._element.className = "slider";

    this._knob = this._element.appendChild(document.createElement("img"));

    this._value = 0;
    this._knobX = 0;
    this.__maxX = 0;

    this._element.addEventListener("mousedown", this);
};

WebInspector.Slider.KnobWidth = 13;

WebInspector.Slider.prototype = {
    contructor: WebInspector.Slider,
    __proto__: WebInspector.Object.prototype,
    
    // Public

    get element()
    {
        return this._element;
    },
    
    get value()
    {
        return this._value;
    },
    
    set value(value)
    {
        value = Math.max(Math.min(value, 1), 0);

        if (value === this._value)
            return;

        this._value = value;

        this._knobX = Math.round(value * this._maxX);
        this._knob.style.webkitTransform = "translate3d(" + this._knobX + "px, 0, 0)";

        if (this.delegate && typeof this.delegate.sliderValueDidChange === "function")
            this.delegate.sliderValueDidChange(this, value);
    },
    
    // Protected

    handleEvent: function(event)
    {
        switch (event.type) {
        case "mousedown":
            this._handleMousedown(event);
            break;
        case "mousemove":
            this._handleMousemove(event);
            break;
        case "mouseup":
            this._handleMouseup(event);
            break;
        }
    },

    // Private

    _handleMousedown: function(event)
    {
        if (event.target !== this._knob)
            this.value = (this._localPointForEvent(event).x - 3) / this._maxX;

        this._startKnobX = this._knobX;
        this._startMouseX = this._localPointForEvent(event).x;

        this._element.classList.add("dragging");

        window.addEventListener("mousemove", this, true);
        window.addEventListener("mouseup", this, true);
    },

    _handleMousemove: function(event)
    {
        var dx = this._localPointForEvent(event).x - this._startMouseX;
        var x = Math.max(Math.min(this._startKnobX + dx, this._maxX), 0);

        this.value = x / this._maxX;
    },

    _handleMouseup: function(event)
    {
        this._element.classList.remove("dragging");

        window.removeEventListener("mousemove", this, true);
        window.removeEventListener("mouseup", this, true);
    },

    _localPointForEvent: function(event)
    {
        // We convert all event coordinates from page coordinates to local coordinates such that the slider
        // may be transformed using CSS Transforms and interaction works as expected.
        return window.webkitConvertPointFromPageToNode(this._element, new WebKitPoint(event.pageX, event.pageY));
    },

    get _maxX()
    {
        if (this.__maxX == 0 && document.body.contains(this._element))
            this.__maxX = this._element.offsetWidth - Math.ceil(WebInspector.Slider.KnobWidth / 2);

        return this.__maxX;
    }
};
