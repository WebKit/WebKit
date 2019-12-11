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

WI.BezierEditor = class BezierEditor extends WI.Object
{
    constructor()
    {
        super();

        this._element = document.createElement("div");
        this._element.classList.add("bezier-editor");

        var editorWidth = 184;
        var editorHeight = 200;
        this._padding = 25;
        this._controlHandleRadius = 7;
        this._bezierWidth = editorWidth - (this._controlHandleRadius * 2);
        this._bezierHeight = editorHeight - (this._controlHandleRadius * 2) - (this._padding * 2);

        this._bezierPreviewContainer = this._element.createChild("div", "bezier-preview");
        this._bezierPreviewContainer.title = WI.UIString("Restart animation");
        this._bezierPreviewContainer.addEventListener("mousedown", this._resetPreviewAnimation.bind(this));

        this._bezierPreview = this._bezierPreviewContainer.createChild("div");

        this._bezierPreviewTiming = this._element.createChild("div", "bezier-preview-timing");

        this._bezierContainer = this._element.appendChild(createSVGElement("svg"));
        this._bezierContainer.setAttribute("width", editorWidth);
        this._bezierContainer.setAttribute("height", editorHeight);
        this._bezierContainer.classList.add("bezier-container");

        let svgGroup = this._bezierContainer.appendChild(createSVGElement("g"));
        svgGroup.setAttribute("transform", "translate(0, " + this._padding + ")");

        let linearCurve = svgGroup.appendChild(createSVGElement("line"));
        linearCurve.classList.add("linear-curve");
        linearCurve.setAttribute("x1", this._controlHandleRadius);
        linearCurve.setAttribute("y1", this._bezierHeight + this._controlHandleRadius);
        linearCurve.setAttribute("x2", this._bezierWidth + this._controlHandleRadius);
        linearCurve.setAttribute("y2", this._controlHandleRadius);

        this._bezierCurve = svgGroup.appendChild(createSVGElement("path"));
        this._bezierCurve.classList.add("bezier-curve");

        function createControl(x1, y1)
        {
            x1 += this._controlHandleRadius;
            y1 += this._controlHandleRadius;

            let line = svgGroup.appendChild(createSVGElement("line"));
            line.classList.add("control-line");
            line.setAttribute("x1", x1);
            line.setAttribute("y1", y1);
            line.setAttribute("x2", x1);
            line.setAttribute("y2", y1);

            let handle = svgGroup.appendChild(createSVGElement("circle"));
            handle.classList.add("control-handle");

            return {point: null, line, handle};
        }

        this._inControl = createControl.call(this, 0, this._bezierHeight);
        this._outControl = createControl.call(this, this._bezierWidth, 0);

        this._numberInputContainer = this._element.createChild("div", "number-input-container");

        function createBezierInput(id, {min, max} = {})
        {
            let key = "_bezier" + id + "Input";
            this[key] = this._numberInputContainer.createChild("input");
            this[key].type = "number";
            this[key].step = 0.01;

            if (!isNaN(min))
                this[key].min = min;

            if (!isNaN(max))
                this[key].max = max;

            this[key].addEventListener("input", this._handleNumberInputInput.bind(this));
            this[key].addEventListener("keydown", this._handleNumberInputKeydown.bind(this));
        }

        createBezierInput.call(this, "InX", {min: 0, max: 1});
        createBezierInput.call(this, "InY");
        createBezierInput.call(this, "OutX", {min: 0, max: 1});
        createBezierInput.call(this, "OutY");

        this._selectedControl = null;
        this._mouseDownPosition = null;
        this._bezierContainer.addEventListener("mousedown", this);

        WI.addWindowKeydownListener(this);
    }

    // Public

    get element()
    {
        return this._element;
    }

    set bezier(bezier)
    {
        if (!bezier)
            return;

        var isCubicBezier = bezier instanceof WI.CubicBezier;
        console.assert(isCubicBezier);
        if (!isCubicBezier)
            return;

        this._bezier = bezier;
        this._updateBezierPreview();
    }

    get bezier()
    {
        return this._bezier;
    }

    removeListeners()
    {
        WI.removeWindowKeydownListener(this);
    }

    // Protected

    handleEvent(event)
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
    }

    handleKeydownEvent(event)
    {
        if (!this._selectedControl || !this._element.parentNode)
            return false;

        let horizontal = 0;
        let vertical = 0;
        switch (event.keyCode) {
        case WI.KeyboardShortcut.Key.Up.keyCode:
            vertical = -1;
            break;
        case WI.KeyboardShortcut.Key.Right.keyCode:
            horizontal = 1;
            break;
        case WI.KeyboardShortcut.Key.Down.keyCode:
            vertical = 1;
            break;
        case WI.KeyboardShortcut.Key.Left.keyCode:
            horizontal = -1;
            break;
        default:
            return false;
        }

        if (event.shiftKey) {
            horizontal *= 10;
            vertical *= 10;
        }

        vertical *= this._bezierWidth / 100;
        horizontal *= this._bezierHeight / 100;

        this._selectedControl.point.x = Number.constrain(this._selectedControl.point.x + horizontal, 0, this._bezierWidth);
        this._selectedControl.point.y += vertical;
        this._updateControl(this._selectedControl);
        this._updateValue();

        return true;
    }

    // Private

    _handleMousedown(event)
    {
        if (event.button !== 0)
            return;

        window.addEventListener("mousemove", this, true);
        window.addEventListener("mouseup", this, true);

        this._bezierPreviewContainer.classList.remove("animate");
        this._bezierPreviewTiming.classList.remove("animate");

        this._updateControlPointsForMouseEvent(event, true);
    }

    _handleMousemove(event)
    {
        this._updateControlPointsForMouseEvent(event);
    }

    _handleMouseup(event)
    {
        this._selectedControl.handle.classList.remove("selected");
        this._mouseDownPosition = null;
        this._triggerPreviewAnimation();

        window.removeEventListener("mousemove", this, true);
        window.removeEventListener("mouseup", this, true);
    }

    _updateControlPointsForMouseEvent(event, calculateSelectedControlPoint)
    {
        var point = WI.Point.fromEventInElement(event, this._bezierContainer);
        point.x = Number.constrain(point.x - this._controlHandleRadius, 0, this._bezierWidth);
        point.y -= this._controlHandleRadius + this._padding;

        if (calculateSelectedControlPoint) {
            this._mouseDownPosition = point;

            if (this._inControl.point.distance(point) < this._outControl.point.distance(point))
                this._selectedControl = this._inControl;
            else
                this._selectedControl = this._outControl;
        }

        if (event.shiftKey && this._mouseDownPosition) {
            if (Math.abs(this._mouseDownPosition.x - point.x) > Math.abs(this._mouseDownPosition.y - point.y))
                point.y = this._mouseDownPosition.y;
            else
                point.x = this._mouseDownPosition.x;
        }

        this._selectedControl.point = point;
        this._selectedControl.handle.classList.add("selected");
        this._updateValue();
    }

    _updateValue()
    {
        function round(num)
        {
            return Math.round(num * 100) / 100;
        }

        var inValueX = round(this._inControl.point.x / this._bezierWidth);
        var inValueY = round(1 - (this._inControl.point.y / this._bezierHeight));

        var outValueX = round(this._outControl.point.x / this._bezierWidth);
        var outValueY = round(1 - (this._outControl.point.y / this._bezierHeight));

        this._bezier = new WI.CubicBezier(inValueX, inValueY, outValueX, outValueY);
        this._updateBezier();

        this.dispatchEventToListeners(WI.BezierEditor.Event.BezierChanged, {bezier: this._bezier});
    }

    _updateBezier()
    {
        var r = this._controlHandleRadius;
        var inControlX = this._inControl.point.x + r;
        var inControlY = this._inControl.point.y + r;
        var outControlX = this._outControl.point.x + r;
        var outControlY = this._outControl.point.y + r;
        var path = `M ${r} ${this._bezierHeight + r} C ${inControlX} ${inControlY} ${outControlX} ${outControlY} ${this._bezierWidth + r} ${r}`;
        this._bezierCurve.setAttribute("d", path);
        this._updateControl(this._inControl);
        this._updateControl(this._outControl);

        this._bezierInXInput.value = this._bezier.inPoint.x;
        this._bezierInYInput.value = this._bezier.inPoint.y;
        this._bezierOutXInput.value = this._bezier.outPoint.x;
        this._bezierOutYInput.value = this._bezier.outPoint.y;
    }

    _updateControl(control)
    {
        control.handle.setAttribute("cx", control.point.x + this._controlHandleRadius);
        control.handle.setAttribute("cy", control.point.y + this._controlHandleRadius);

        control.line.setAttribute("x2", control.point.x + this._controlHandleRadius);
        control.line.setAttribute("y2", control.point.y + this._controlHandleRadius);
    }

    _updateBezierPreview()
    {
        this._inControl.point = new WI.Point(this._bezier.inPoint.x * this._bezierWidth, (1 - this._bezier.inPoint.y) * this._bezierHeight);
        this._outControl.point = new WI.Point(this._bezier.outPoint.x * this._bezierWidth, (1 - this._bezier.outPoint.y) * this._bezierHeight);

        this._updateBezier();
        this._triggerPreviewAnimation();
    }

    _triggerPreviewAnimation()
    {
        this._bezierPreview.style.animationTimingFunction = this._bezier.toString();
        this._bezierPreviewContainer.classList.add("animate");
        this._bezierPreviewTiming.classList.add("animate");
    }

    _resetPreviewAnimation()
    {
        var parent = this._bezierPreview.parentNode;
        parent.removeChild(this._bezierPreview);
        parent.appendChild(this._bezierPreview);

        this._element.removeChild(this._bezierPreviewTiming);
        this._element.appendChild(this._bezierPreviewTiming);
    }

    _handleNumberInputInput(event)
    {
        this._changeBezierForInput(event.target, event.target.value);
    }

    _handleNumberInputKeydown(event)
    {
        let shift = 0;
        if (event.keyIdentifier === "Up")
            shift = 0.01;
         else if (event.keyIdentifier === "Down")
            shift = -0.01;

        if (!shift)
            return;

        if (event.shiftKey)
            shift *= 10;

        event.preventDefault();
        this._changeBezierForInput(event.target, parseFloat(event.target.value) + shift);
    }

    _changeBezierForInput(target, value)
    {
        value = Math.round(value * 100) / 100;

        switch (target) {
        case this._bezierInXInput:
            this._bezier.inPoint.x = Number.constrain(value, 0, 1);
            break;
        case this._bezierInYInput:
            this._bezier.inPoint.y = value;
            break;
        case this._bezierOutXInput:
            this._bezier.outPoint.x = Number.constrain(value, 0, 1);
            break;
        case this._bezierOutYInput:
            this._bezier.outPoint.y = value;
            break;
        default:
            return;
        }

        this._updateBezierPreview();

        this.dispatchEventToListeners(WI.BezierEditor.Event.BezierChanged, {bezier: this._bezier});
    }
};

WI.BezierEditor.Event = {
    BezierChanged: "bezier-editor-bezier-changed"
};
