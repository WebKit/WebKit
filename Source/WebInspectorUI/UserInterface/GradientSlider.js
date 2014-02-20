/*
 * Copyright (C) 2014 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

WebInspector.GradientSlider = function(delegate)
{
    this.delegate = delegate;
    
    this._element = null;
    this._stops = [];
    this._knobs = [];
    
    this._selectedKnob = null;
    this._canvas = document.createElement("canvas");

    this._keyboardShortcutEsc = new WebInspector.KeyboardShortcut(null, WebInspector.KeyboardShortcut.Key.Escape);
}

WebInspector.GradientSlider.Width = 238;
WebInspector.GradientSlider.Height = 19;

WebInspector.GradientSlider.StyleClassName = "gradient-slider";
WebInspector.GradientSlider.AddAreaClassName = "add-area";
WebInspector.GradientSlider.DetachingClassName = "detaching";
WebInspector.GradientSlider.ShadowClassName = "shadow";

WebInspector.GradientSlider.prototype = {
    constructor: WebInspector.GradientSlider,

    // Public

    get element()
    {
        if (!this._element) {
            this._element = document.createElement("div");
            this._element.className = WebInspector.GradientSlider.StyleClassName;
            this._element.appendChild(this._canvas);
            
            this._addArea = this._element.appendChild(document.createElement("div"));
            this._addArea.addEventListener("mouseover", this);
            this._addArea.addEventListener("mousemove", this);
            this._addArea.addEventListener("mouseout", this);
            this._addArea.addEventListener("click", this);
            this._addArea.className = WebInspector.GradientSlider.AddAreaClassName;
        }
        return this._element;
    },

    get stops()
    {
        return this._stops;
    },

    set stops(stops)
    {
        this._stops = stops;

        this._updateStops();
    },

    get selectedStop()
    {
        return this._selectedKnob ? this._selectedKnob.stop : null;
    },

    // Protected

    handleEvent: function(event)
    {
        switch (event.type) {
        case "mouseover":
            this._handleMouseover(event);
            break;
        case "mousemove":
            this._handleMousemove(event);
            break;
        case "mouseout":
            this._handleMouseout(event);
            break;
        case "click":
            this._handleClick(event);
            break;
        }
    },

    handleKeydownEvent: function(event)
    {
        if (!this._keyboardShortcutEsc.matchesEvent(event) || !this._selectedKnob || !this._selectedKnob.selected)
            return false;

        this._selectedKnob.selected = false;

        return true;
    },

    knobXDidChange: function(knob)
    {
        knob.stop.offset = knob.x / WebInspector.GradientSlider.Width;
        this._sortStops();
        this._updateCanvas();
    },

    knobCanDetach: function(knob)
    {
        return this._knobs.length > 2;
    },

    knobWillDetach: function(knob)
    {
        knob.element.classList.add(WebInspector.GradientSlider.DetachingClassName);
        
        this._stops.remove(knob.stop);
        this._knobs.remove(knob);
        this._sortStops();
        this._updateCanvas();
    },

    knobSelectionChanged: function(knob)
    {
        if (this._selectedKnob && this._selectedKnob !== knob && knob.selected)
            this._selectedKnob.selected = false;

        this._selectedKnob = knob.selected ? knob : null;

        if (this.delegate && typeof this.delegate.gradientSliderStopWasSelected === "function")
            this.delegate.gradientSliderStopWasSelected(this, knob.stop);

        if (this._selectedKnob)
            WebInspector.addWindowKeydownListener(this);
        else
            WebInspector.removeWindowKeydownListener(this);
    },

    // Private

    _handleMouseover: function(event)
    {
        this._updateShadowKnob(event);
    },

    _handleMousemove: function(event)
    {
        this._updateShadowKnob(event);
    },

    _handleMouseout: function(event)
    {
        if (!this._shadowKnob)
            return;

        this._shadowKnob.element.remove();
        delete this._shadowKnob;
    },

    _handleClick: function(event)
    {
        this._updateShadowKnob(event);

        this._knobs.push(this._shadowKnob);

        this._shadowKnob.element.classList.remove(WebInspector.GradientSlider.ShadowClassName);

        var stop = {offset: this._shadowKnob.x / WebInspector.GradientSlider.Width, color: this._shadowKnob.wellColor};
        this._stops.push(stop);
        this._sortStops();
        this._updateStops();

        this._knobs[this._stops.indexOf(stop)].selected = true;

        delete this._shadowKnob;
    },

    _updateShadowKnob: function(event)
    {
        if (!this._shadowKnob) {
            this._shadowKnob = new WebInspector.GradientSliderKnob(this);
            this._shadowKnob.element.classList.add(WebInspector.GradientSlider.ShadowClassName);
            this.element.appendChild(this._shadowKnob.element);
        }
        
        this._shadowKnob.x = window.webkitConvertPointFromPageToNode(this.element, new WebKitPoint(event.pageX, event.pageY)).x;

        var colorData = this._canvas.getContext("2d").getImageData(this._shadowKnob.x - 1, 0, 1, 1).data;
        this._shadowKnob.wellColor = new WebInspector.Color(WebInspector.Color.Format.RGB, [colorData[0], colorData[1], colorData[2], colorData[3] / 255]);
    },

    _sortStops: function()
    {
        this._stops.sort(function(a, b) {
            return a.offset - b.offset;
        });
    },

    _updateStops: function()
    {
        this._updateCanvas();
        this._updateKnobs();
    },

    _updateCanvas: function()
    {
        var w = WebInspector.GradientSlider.Width;
        var h = WebInspector.GradientSlider.Height;
        
        this._canvas.width = w;
        this._canvas.height = h;

        var ctx = this._canvas.getContext("2d");
        var gradient = ctx.createLinearGradient(0, 0, w, 0);
        for (var stop of this._stops)
            gradient.addColorStop(stop.offset, stop.color);

        ctx.clearRect(0, 0, w, h);
        ctx.fillStyle = gradient;
        ctx.fillRect(0, 0, w, h);

        if (this.delegate && typeof this.delegate.gradientSliderStopsDidChange === "function")
            this.delegate.gradientSliderStopsDidChange(this);
    },

    _updateKnobs: function()
    {
        var selectedStop = this._selectedKnob ? this._selectedKnob.stop : null;
        
        while (this._knobs.length > this._stops.length)
            this._knobs.pop().element.remove();

        while (this._knobs.length < this._stops.length) {
            var knob = new WebInspector.GradientSliderKnob(this);
            this.element.appendChild(knob.element);
            this._knobs.push(knob);
        }

        for (var i = 0; i < this._stops.length; ++i) {
            var stop = this._stops[i];
            var knob = this._knobs[i];

            knob.stop = stop;
            knob.x = Math.round(stop.offset * WebInspector.GradientSlider.Width);
            knob.selected = stop === selectedStop;
        }
    }
}

WebInspector.GradientSliderKnob = function(delegate)
{
    this._x = 0;
    this._y = 0;
    this._stop = null;
    
    this.delegate = delegate;
    
    this._element = document.createElement("div");
    this._element.className = WebInspector.GradientSliderKnob.StyleClassName;

    // Checkers pattern.
    this._element.appendChild(document.createElement("img"));

    this._well = this._element.appendChild(document.createElement("div"));

    this._element.addEventListener("mousedown", this);
};

WebInspector.GradientSliderKnob.StyleClassName = "gradient-slider-knob";
WebInspector.GradientSliderKnob.SelectedClassName = "selected";
WebInspector.GradientSliderKnob.FadeOutClassName = "fade-out";

WebInspector.GradientSliderKnob.prototype = {
    constructor: WebInspector.GradientSliderKnob,

    // Public

    get element()
    {
        return this._element;
    },

    get stop()
    {
        return this._stop;
    },

    set stop(stop)
    {
        this.wellColor = stop.color;
        this._stop = stop;
    },

    get x()
    {
        return this._x;
    },
    
    set x(x) {
        this._x = x;
        this._updateTransform();
    },

    get y()
    {
        return this._x;
    },
    
    set y(y) {
        this._y = y;
        this._updateTransform();
    },

    get wellColor(color)
    {
        return this._wellColor;
    },

    set wellColor(color)
    {
        this._wellColor = color;
        this._well.style.backgroundColor = color;
    },

    get selected()
    {
        return this._element.classList.contains(WebInspector.GradientSliderKnob.SelectedClassName);
    },

    set selected(selected)
    {
        if (this.selected === selected)
            return;

        this._element.classList.toggle(WebInspector.GradientSliderKnob.SelectedClassName, selected);
        
        if (this.delegate && typeof this.delegate.knobSelectionChanged === "function")
            this.delegate.knobSelectionChanged(this);
    },

    // Protected

    handleEvent: function(event)
    {
        event.preventDefault();
        event.stopPropagation();

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
        case "transitionend":
            this._handleTransitionEnd(event);
            break;
        }
    },

    // Private

    _handleMousedown: function(event)
    {
        this._moved = false;
        this._detaching = false;

        window.addEventListener("mousemove", this, true);
        window.addEventListener("mouseup", this, true);

        this._startX = this.x;
        this._startMouseX = event.pageX;
        this._startMouseY = event.pageY;
    },

    _handleMousemove: function(event)
    {
        var w = WebInspector.GradientSlider.Width;

        this._moved = true;

        if (!this._detaching && Math.abs(event.pageY - this._startMouseY) > 50) {
            this._detaching = this.delegate && typeof this.delegate.knobCanDetach === "function" && this.delegate.knobCanDetach(this);
            if (this._detaching && this.delegate && typeof this.delegate.knobWillDetach === "function") {
                var translationFromParentToBody = window.webkitConvertPointFromNodeToPage(this.element.parentNode, new WebKitPoint(0, 0));
                this._startMouseX -= translationFromParentToBody.x;
                this._startMouseY -= translationFromParentToBody.y;
                document.body.appendChild(this.element);
                this.delegate.knobWillDetach(this);
            }
        }

        var x = this._startX + event.pageX - this._startMouseX;
        if (!this._detaching)
            x = Math.min(Math.max(0, x), w);
        this.x = x;

        if (this._detaching)
            this.y = event.pageY - this._startMouseY;
        else if (this.delegate && typeof this.delegate.knobXDidChange === "function")
            this.delegate.knobXDidChange(this);
    },

    _handleMouseup: function(event)
    {
        window.removeEventListener("mousemove", this, true);
        window.removeEventListener("mouseup", this, true);

        if (this._detaching) {
            this.element.addEventListener("transitionend", this);
            this.element.classList.add(WebInspector.GradientSliderKnob.FadeOutClassName);
            this.selected = false;
        } else if (!this._moved)
            this.selected = !this.selected;
    },

    _handleTransitionEnd: function(event)
    {
        this.element.removeEventListener("transitionend", this);
        this.element.classList.remove(WebInspector.GradientSliderKnob.FadeOutClassName);
        this.element.remove();
    },

    _updateTransform: function()
    {
        this.element.style.webkitTransform = "translate3d(" + this._x + "px, " + this._y + "px, 0)";
    }
}
