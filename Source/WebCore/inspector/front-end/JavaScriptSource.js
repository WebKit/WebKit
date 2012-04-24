/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

/**
 * @constructor
 * @extends {WebInspector.UISourceCode}
 * @param {string} id
 * @param {string} url
 * @param {WebInspector.ContentProvider} contentProvider
 */
WebInspector.JavaScriptSource = function(id, url, contentProvider)
{
    WebInspector.UISourceCode.call(this, id, url, contentProvider);

    this._model = WebInspector.debuggerPresentationModel;

    if (this._model) // For tests.
        this._breakpointManager = this._model.breakpointManager;

    /**
     * @type {Object.<string,WebInspector.UIBreakpoint>}
     */
    this._breakpoints = {};
    /**
     * @type {Array.<WebInspector.PresentationConsoleMessage>}
     */
    this._consoleMessages = [];
}

WebInspector.JavaScriptSource.prototype = {
    /**
     * @return {Object.<string,WebInspector.UIBreakpoint>}
     */
    breakpoints: function()
    {
        return this._breakpoints;
    },

    /**
     * @param {number} lineNumber
     * @param {WebInspector.UIBreakpoint} breakpoint
     */
    breakpointAdded: function(lineNumber, breakpoint)
    {
        console.assert(!this._breakpoints[String(lineNumber)]);
        this._breakpoints[String(lineNumber)] = breakpoint;
        this.dispatchEventToListeners(WebInspector.UISourceCode.Events.BreakpointAdded, breakpoint);
    },

    /**
     * @param {number} lineNumber
     */
    breakpointRemoved: function(lineNumber)
    {
        var breakpoint = this._breakpoints[String(lineNumber)];
        delete this._breakpoints[String(lineNumber)];
        this.dispatchEventToListeners(WebInspector.UISourceCode.Events.BreakpointRemoved, breakpoint);
    },

    /**
     * @return {Array.<WebInspector.PresentationConsoleMessage>}
     */
    consoleMessages: function()
    {
        return this._consoleMessages;
    },

    /**
     * @param {WebInspector.PresentationConsoleMessage} message
     */
    consoleMessageAdded: function(message)
    {
        this._consoleMessages.push(message);
        this.dispatchEventToListeners(WebInspector.UISourceCode.Events.ConsoleMessageAdded, message);
    },

    consoleMessagesCleared: function()
    {
        this._consoleMessages = [];
        this.dispatchEventToListeners(WebInspector.UISourceCode.Events.ConsoleMessagesCleared);
    },

    /**
     * @param {number} lineNumber
     * @return {WebInspector.UIBreakpoint|undefined}
     */
    findBreakpoint: function(lineNumber)
    {
        return this._breakpoints[String(lineNumber)];
    },

    /**
     * @param {number} lineNumber
     * @param {string} condition
     * @param {boolean} enabled
     */
    setBreakpoint: function(lineNumber, condition, enabled)
    {
        this._breakpointManager.setBreakpoint(this, lineNumber, condition, enabled);
        this._model.setBreakpointsActive(true);
    },

    /**
     * @param {number} lineNumber
     * @param {boolean} enabled
     */
    setBreakpointEnabled: function(lineNumber, enabled)
    {
        var breakpoint = this.findBreakpoint(lineNumber);
        if (!breakpoint)
            return;
        this._breakpointManager.removeBreakpoint(this, lineNumber);
        this._breakpointManager.setBreakpoint(this, lineNumber, breakpoint.condition, enabled);
    },

    /**
     * @param {number} lineNumber
     */
    removeBreakpoint: function(lineNumber)
    {
        this._breakpointManager.removeBreakpoint(this, lineNumber);
    },

    /**
     * @param {number} lineNumber
     * @param {string} condition
     * @param {boolean} enabled
     */
    updateBreakpoint: function(lineNumber, condition, enabled)
    {
        this._breakpointManager.removeBreakpoint(this, lineNumber);
        this._breakpointManager.setBreakpoint(this, lineNumber, condition, enabled);
    },

    continueToLine: function(lineNumber)
    {
        this._model.continueToLine(this, lineNumber);
    },

    /**
     * @return {boolean}
     */
    canSetContent: function()
    {
        return this._model.canEditScriptSource(this);
    },

    /**
     * @param {string} newSource
     * @param {function(?Protocol.Error)} callback
     */
    setContent: function(newSource, callback)
    {
        this._model.setScriptSource(this, newSource, callback);
    },

    /**
     * @param {string} oldSource
     * @param {string} newSource
     */
    updateBreakpointsAfterLiveEdit: function(oldSource, newSource)
    {
        // Clear and re-create breakpoints according to text diff.
        var diff = Array.diff(oldSource.split("\n"), newSource.split("\n"));
        for (var lineNumber in this._breakpoints) {
            var breakpoint = this._breakpoints[lineNumber];

            this.removeBreakpoint(parseInt(lineNumber, 10));

            var newLineNumber = diff.left[lineNumber].row;
            if (newLineNumber === undefined) {
                for (var i = lineNumber - 1; i >= 0; --i) {
                    if (diff.left[i].row === undefined)
                        continue;
                    var shiftedLineNumber = diff.left[i].row + lineNumber - i;
                    if (shiftedLineNumber < diff.right.length) {
                        var originalLineNumber = diff.right[shiftedLineNumber].row;
                        if (originalLineNumber === lineNumber || originalLineNumber === undefined)
                            newLineNumber = shiftedLineNumber;
                    }
                    break;
                }
            }
            if (newLineNumber !== undefined)
                this.setBreakpoint(newLineNumber, breakpoint.condition, breakpoint.enabled);
        }
    }
}

WebInspector.JavaScriptSource.prototype.__proto__ = WebInspector.UISourceCode.prototype;
