/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

WebInspector.Breakpoint = function(debuggerModel, sourceID, url, line, enabled, condition)
{
    this.url = url;
    this.line = line;
    this.sourceID = sourceID;
    this._enabled = enabled;
    this._condition = condition || "";
    this._sourceText = "";
    this._hit = false;
    this._debuggerModel = debuggerModel;
}

WebInspector.Breakpoint.jsBreakpointId = function(sourceID, line)
{
    return sourceID + ":" + line;
}

WebInspector.Breakpoint.prototype = {
    get enabled()
    {
        return this._enabled;
    },

    set enabled(x)
    {
        if (this._enabled === x)
            return;

        this._enabled = x;
        this._debuggerModel._setBreakpointOnBackend(this);
        this.dispatchEventToListeners("enable-changed");
    },

    get sourceText()
    {
        return this._sourceText;
    },

    set sourceText(text)
    {
        this._sourceText = text;
        this.dispatchEventToListeners("label-changed");
    },

    get id()
    {
        return WebInspector.Breakpoint.jsBreakpointId(this.sourceID, this.line);
    },

    get condition()
    {
        return this._condition;
    },

    set condition(c)
    {
        c = c || "";
        if (this._condition === c)
            return;

        this._condition = c;
        if (this.enabled)
            this._debuggerModel._setBreakpointOnBackend(this);
        this.dispatchEventToListeners("condition-changed");
    },

    get hit()
    {
        return this._hit;
    },

    set hit(hit)
    {
        this._hit = hit;
        this.dispatchEventToListeners("hit-state-changed");
    },

    click: function(event)
    {
        WebInspector.panels.scripts.showSourceLine(this.url, this.line);
    },

    compareTo: function(other)
    {
        if (this.url != other.url)
            return this.url < other.url ? -1 : 1;
        if (this.line != other.line)
            return this.line < other.line ? -1 : 1;
        return 0;
    },

    populateLabelElement: function(element)
    {
        var displayName = this.url ? WebInspector.displayNameForURL(this.url) : WebInspector.UIString("(program)");
        var labelElement = document.createTextNode(displayName + ":" + this.line);
        element.appendChild(labelElement);

        var sourceTextElement = document.createElement("div");
        sourceTextElement.textContent = this.sourceText;
        sourceTextElement.className = "source-text monospace";
        element.appendChild(sourceTextElement);
    },

    remove: function()
    {
        InspectorBackend.removeBreakpoint(this.sourceID, this.line);
        this.dispatchEventToListeners("removed");
        this.removeAllListeners();
        delete this._debuggerModel;
    }
}

WebInspector.Breakpoint.prototype.__proto__ = WebInspector.Object.prototype;
