/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

WebInspector.SourceFrame2 = function()
{
    this._editor = new WebInspector.TextEditor(WebInspector.platform);
    this._textModel = this._editor.textModel;
    this._editor.lineNumberDecorator = new WebInspector.BreakpointLineNumberDecorator(this);
    this._editor.lineDecorator = new WebInspector.ExecutionLineDecorator(this);
    this._editor.readOnly = true;
    this.element = this._editor.element;
}

WebInspector.SourceFrame2.prototype = {
    set text(text)
    {
        this._editor.text = text;
    },

    get executionLine()
    {
        return this._executionLine;
    },

    set executionLine(x)
    {
        this._executionLine = x;
    },

    revealLine: function(lineNumber)
    {
        this._editor.reveal(lineNumber, 0);
    },

    _toggleBreakpoint: function(lineNumber)
    {
        if (this._textModel.getAttribute(lineNumber, "breakpoint"))
            this._textModel.removeAttribute(lineNumber, "breakpoint");
        else
            this._textModel.setAttribute(lineNumber, "breakpoint", { disabled: false, conditional: false });
        this._editor.paintLineNumbers();
    },

    resize: function()
    {
        this._editor.updateCanvasSize();
    }
}

WebInspector.SourceFrame2.prototype.__proto__ = WebInspector.Object.prototype;

WebInspector.BreakpointLineNumberDecorator = function(sourceFrame)
{
    this._sourceFrame = sourceFrame;
    this._textModel = sourceFrame._editor.textModel;
}

WebInspector.BreakpointLineNumberDecorator.prototype = {
    decorate: function(lineNumber, ctx, x, y, width, height, lineHeight)
    {
        var breakpoint = this._textModel.getAttribute(lineNumber, "breakpoint");
        var isExecutionLine = lineNumber === this._sourceFrame._executionLine;
        if (breakpoint || isExecutionLine) {
            ctx.save();
            ctx.translate(x + 4, y + 2);
            var breakpointWidth = width - 6;
            var breakpointHeight = lineHeight - 4;
    
            if (breakpoint)
                this._paintBreakpoint(ctx, breakpointWidth, breakpointHeight, breakpoint.conditional, breakpoint.disabled);
    
            if (isExecutionLine)
                this._paintProgramCounter(ctx, breakpointWidth, breakpointHeight, false);

            ctx.restore();
        }

        if (isExecutionLine) {
            // Override default behavior.
            return true;
        }

        ctx.fillStyle = breakpoint ? "rgb(255,255,255)" : "rgb(155,155,155)";
        return false;
    },

    _paintBreakpoint: function(ctx, width, height, conditional, disabled)
    {
        ctx.beginPath();
        ctx.moveTo(0, 2);
        ctx.lineTo(2, 0);
        ctx.lineTo(width - 5, 0);
        ctx.lineTo(width, height / 2);
        ctx.lineTo(width - 5, height);
        ctx.lineTo(2, height);
        ctx.lineTo(0, height - 2);
        ctx.closePath();
        ctx.fillStyle = conditional ? "rgb(217, 142, 1)" : "rgb(1, 142, 217)";
        ctx.strokeStyle = conditional ? "rgb(205, 103, 0)" : "rgb(0, 103, 205)";
        ctx.lineWidth = 3;
        ctx.fill();

        ctx.save();
        ctx.clip();
        ctx.stroke();
        ctx.restore();

        if (disabled) {
            ctx.save();
            ctx.globalCompositeOperation = "destination-out";
            ctx.fillStyle = "rgba(0, 0, 0, 0.5)";
            ctx.fillRect(0, 0, breakpointWidth, breakpointHeight);
            ctx.restore();
        }
    },

    _paintProgramCounter: function(ctx, width, height)
    {
        ctx.save();

        ctx.beginPath();
        ctx.moveTo(width - 9, 2);
        ctx.lineTo(width - 7, 2);
        ctx.lineTo(width - 7, 0);
        ctx.lineTo(width - 5, 0);
        ctx.lineTo(width, height / 2);
        ctx.lineTo(width - 5, height);
        ctx.lineTo(width - 7, height);
        ctx.lineTo(width - 7, height - 2);
        ctx.lineTo(width - 9, height - 2);
        ctx.closePath();
        ctx.fillStyle = "rgb(142, 5, 4)";

        ctx.shadowBlur = 4;
        ctx.shadowColor = "rgb(255, 255, 255)";
        ctx.shadowOffsetX = -1;
        ctx.shadowOffsetY = 0;

        ctx.fill();
        ctx.fill(); // Fill twice to get a good shadow and darker anti-aliased pixels.

        ctx.restore();
    },

    mouseDown: function(lineNumber, e)
    {
        this._sourceFrame._toggleBreakpoint(lineNumber);
        return true;
    }
}

WebInspector.ExecutionLineDecorator = function(sourceFrame)
{
    this._sourceFrame = sourceFrame;
}

WebInspector.ExecutionLineDecorator.prototype = {
    decorate: function(lineNumber, ctx, x, y, width, height, lineHeight)
    {
        if (this._sourceFrame._executionLine !== lineNumber)
            return;
        ctx.save();
        ctx.fillStyle = "rgb(171, 191, 254)";
        ctx.fillRect(x, y, width, height);
        
        ctx.beginPath();
        ctx.rect(x - 1, y, width + 2, height);
        ctx.clip();
        ctx.strokeStyle = "rgb(64, 115, 244)";
        ctx.stroke();

        ctx.restore();
    }
}
