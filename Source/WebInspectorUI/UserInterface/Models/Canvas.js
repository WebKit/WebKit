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

WebInspector.Canvas = class Canvas extends WebInspector.Object
{
    constructor(id, parentFrame, name, cssCanvas, contextType, contextAttributes, programs)
    {
        super();

        console.assert(id);
        console.assert(parentFrame);
        console.assert(contextType);
        console.assert(this._contextType !== WebInspector.Canvas.ContextType.WebGL || contextAttributes);

        this._id = id;
        this._parentFrame = parentFrame;
        this._name = name;
        this._cssCanvas = cssCanvas;
        this._contextType = contextType;
        this._contextAttributes = contextAttributes;
        this._programs = new Map;

        if (programs) {
            for (var program of programs) {
                program.updateCanvas(this);
                this._programs.set(program.id.objectId, program);
            }
        }
    }

    // Static

    static fromPayload(payload)
    {
        var parentFrame = WebInspector.frameResourceManager.frameForIdentifier(payload.frameId);
        var contextType = null;
        switch (payload.contextType) {
            case CanvasAgent.ContextType.Canvas2D:
                contextType = WebInspector.Canvas.ContextType.Canvas2D;
                break;
            case CanvasAgent.ContextType.WebGL:
                contextType = WebInspector.Canvas.ContextType.WebGL;
                break;
            default:
                console.error("Invalid canvas context type", payload.contextType);
        }

        var programs = [];
        for (var programId of payload.programIds)
            programs.push(new WebInspector.ShaderProgram(programId));

        var contextAttributes = null;
        if (payload.contextAttributes) {
            console.assert(contextType === WebInspector.Canvas.ContextType.WebGL);
            contextAttributes = WebInspector.WebGLContextAttributes.fromPayload(payload.contextAttributes);
        }

        return new WebInspector.Canvas(payload.canvasId, parentFrame, payload.name, payload.cssCanvas, contextType, contextAttributes, programs);
    }

    static displayNameForContextType(contextType)
    {
        switch (contextType) {
            case WebInspector.Canvas.ContextType.Canvas2D:
                return WebInspector.UIString("2D");
            case WebInspector.Canvas.ContextType.WebGL:
                return WebInspector.UIString("WebGL");
            default:
                console.error("Invalid canvas context type", contextType);
        }
    }

    static resetUniqueDisplayNameNumbers()
    {
        WebInspector.Canvas._nextUniqueDisplayNameNumber = 1;
    }

    // Public

    get id()
    {
        return this._id;
    }

    get parentFrame()
    {
        return this._parentFrame;
    }

    get name()
    {
        return this._name;
    }

    get cssCanvas()
    {
        return this._cssCanvas;
    }

    get contextType()
    {
        return this._contextType;
    }

    get contextAttributes()
    {
        return this._contextAttributes;
    }

    get programs()
    {
        var programs = [];
        for (var program of this._programs.values())
            programs.push(program);
        return programs;
    }

    get displayName()
    {
        if (this._cssCanvas)
            return WebInspector.UIString("CSS canvas “%s”").format(this._name);

        if (this._name)
            return WebInspector.UIString("Canvas #%s").format(this._name);

        if (!this._uniqueDisplayNameNumber)
            this._uniqueDisplayNameNumber = this.constructor._nextUniqueDisplayNameNumber++;
        return WebInspector.UIString("Canvas %d").format(this._uniqueDisplayNameNumber);
    }

    programForId(programIdentifier)
    {
        return this._programs.get(programIdentifier.objectId);
    }

    programWasCreated(program)
    {
        console.assert(!this._programs.has(program.id.objectId));
        this._programs.set(program.id.objectId, program);

        this.dispatchEventToListeners(WebInspector.Canvas.Event.ProgramWasCreated, {program: program});
    }

    programWasDeleted(programIdentifier)
    {
        var program = this._programs.take(programIdentifier.objectId);
        console.assert(program);

        this.dispatchEventToListeners(WebInspector.Canvas.Event.ProgramWasDeleted, {program: program});
    }

    saveIdentityToCookie(cookie)
    {
        cookie[WebInspector.Canvas.FrameURLCookieKey] = this.parentFrame.url.hash;
        cookie[WebInspector.Canvas.CSSCanvasCookieKey] = this._cssCanvas;
        cookie[WebInspector.Canvas.NameCookieKey] = this._name;
    }
};

WebInspector.Canvas.Event = {
    ProgramWasCreated: "canvas-program-was-created",
    ProgramWasDeleted: "canvas-program-was-deleted",
};

WebInspector.Canvas.ContextType = {
    Canvas2D: Symbol("canvas-context-type-canvas-2d"),
    WebGL: Symbol("canvas-context-type-webgl"),
};

WebInspector.Canvas._nextUniqueDisplayNameNumber = 1;

WebInspector.Canvas.FrameURLCookieKey = "canvas-frame-url";
WebInspector.Canvas.CSSCanvasCookieKey = "canvas-css-canvas";
WebInspector.Canvas.NameCookieKey = "canvas-name";
