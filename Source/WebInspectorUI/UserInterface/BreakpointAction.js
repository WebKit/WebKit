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

WebInspector.BreakpointAction = function(breakpoint, typeOrInfo, data)
{
    WebInspector.Object.call(this);

    console.assert(breakpoint);
    console.assert(typeOrInfo);

    this._breakpoint = breakpoint;

    if (typeof typeOrInfo === "string") {
        this._type = typeOrInfo;
        this._data = data || null;
    } else if (typeof typeOrInfo === "object") {
        this._type = typeOrInfo.type;
        this._data = typeOrInfo.data || null;
    } else
        console.error("Unexpected type passed to WebInspector.BreakpointAction");

    console.assert(typeof this._type === "string");
};

WebInspector.BreakpointAction.Type = {
    Log: "log",
    Evaluate: "evaluate",
    Sound: "sound",
    Probe: "probe"
}

WebInspector.BreakpointAction.prototype = {
    constructor: WebInspector.BreakpointAction,

    // Public

    get breakpoint()
    {
        return this._breakpoint;
    },

    get type()
    {
        return this._type;
    },

    get data()
    {
        return this._data;
    },

    set data(data)
    {
        if (this._data === data)
            return;

        this._data = data;

        this._breakpoint.breakpointActionDidChange(this);
    },

    get info()
    {
        var obj = {type: this._type};
        if (this._data)
            obj.data = this._data;
        return obj;
    }
};

WebInspector.BreakpointAction.prototype.__proto__ = WebInspector.Object.prototype;
