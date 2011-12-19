/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
 * @param {WebInspector.Setting} breakpointStorage
 * @param {function(WebInspector.Breakpoint)} breakpointAddedDelegate
 * @param {function(WebInspector.Breakpoint)} breakpointRemovedDelegate
 * @param {WebInspector.DebuggerModel} debuggerModel
 */
WebInspector.BreakpointManager = function(breakpointStorage, breakpointAddedDelegate, breakpointRemovedDelegate, debuggerModel)
{
    this._breakpointStorage = breakpointStorage;
    this._breakpointAddedDelegate = breakpointAddedDelegate;
    this._breakpointRemovedDelegate = breakpointRemovedDelegate;
    /**
     * @type {Object.<string, Object.<string,WebInspector.Breakpoint>>}
     */
    this._breakpointsByUILocation = {};

    this._debuggerModel = debuggerModel;

    /**
     * @type {Object.<DebuggerAgent.BreakpointId, WebInspector.Breakpoint>}
     */
    this._breakpointsByDebuggerId = {};
    this._debuggerModel.addEventListener(WebInspector.DebuggerModel.Events.BreakpointResolved, this._breakpointResolved, this);

    var breakpoints = this._breakpointStorage.get();
    for (var i = 0; i < breakpoints.length; ++i) {
        var breakpoint = WebInspector.Breakpoint.deserialize(breakpoints[i]);
        if (!this._breakpoint(breakpoint.uiSourceCodeId, breakpoint.lineNumber))
            this._addBreakpointToUI(breakpoint);
    }
}

WebInspector.BreakpointManager.prototype = {
    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     */
    uiSourceCodeAdded: function(uiSourceCode)
    {
        var breakpoints = this._breakpoints(uiSourceCode.id);
        for (var lineNumber in breakpoints) {
            var breakpoint = breakpoints[lineNumber];
            breakpoint.uiSourceCode = uiSourceCode;
            this._materializeBreakpoint(breakpoint, uiSourceCode.rawSourceCode.sourceMapping, uiSourceCode);
            if (breakpoint._debuggerLocation)
                this._breakpointDebuggerLocationChanged(breakpoint);
        }
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     */
    breakpointsForUISourceCode: function(uiSourceCode)
    {
        return this._breakpoints(uiSourceCode.id);
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     * @param {number} lineNumber
     * @param {string} condition
     * @param {boolean} enabled
     */
    setBreakpoint: function(uiSourceCode, lineNumber, condition, enabled)
    {
        if (this._breakpoint(uiSourceCode.id, lineNumber))
            return;

        var persistent = !!uiSourceCode.url;
        var breakpoint = new WebInspector.Breakpoint(uiSourceCode.id, lineNumber, condition, enabled, persistent);
        breakpoint.uiSourceCode = uiSourceCode;
        this._addBreakpointToUI(breakpoint);
        this._materializeBreakpoint(breakpoint, uiSourceCode.rawSourceCode.sourceMapping, uiSourceCode);
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     * @param {number} lineNumber
     */
    removeBreakpoint: function(uiSourceCode, lineNumber)
    {
        var breakpoint = this._breakpoint(uiSourceCode.id, lineNumber);
        if (!breakpoint)
            return;
        this._removeBreakpoint(breakpoint);
    },

    /**
     * @param {WebInspector.Breakpoint} breakpoint
     */
    _removeBreakpoint: function(breakpoint)
    {
        this._deleteBreakpointFromUI(breakpoint);
        this._removeBreakpointFromDebugger(breakpoint);
    },

    /**
     */
    removeAllBreakpoints: function()
    {
        this._forEachBreakpoint(this._removeBreakpoint.bind(this));
    },

    /**
     * @param {WebInspector.Breakpoint} breakpoint
     * @param {WebInspector.RawSourceCode.SourceMapping} sourceMapping
     * @param {WebInspector.UISourceCode} uiSourceCode
     */
    _materializeBreakpoint: function(breakpoint, sourceMapping, uiSourceCode)
    {
        if (!breakpoint.enabled || breakpoint._materialized)
            return;

        breakpoint._materialized = true;
        var rawLocation = sourceMapping.uiLocationToRawLocation(uiSourceCode, breakpoint.lineNumber, 0);
        this._setBreakpointInDebugger(breakpoint, rawLocation);
    },

    /**
     * @param {WebInspector.Breakpoint} breakpoint
     */
    _breakpointDebuggerLocationChanged: function(breakpoint)
    {
        if (!breakpoint.uiSourceCode)
            return;
        var uiLocation = breakpoint.uiSourceCode.rawSourceCode.sourceMapping.rawLocationToUILocation(breakpoint._debuggerLocation);
        if (uiLocation.lineNumber === breakpoint.lineNumber)
            return;

        if (!this._moveBreakpointInUI(breakpoint, uiLocation.lineNumber))
            this._removeBreakpointFromDebugger(breakpoint);
    },

    /**
     * @param {WebInspector.Breakpoint} breakpoint
     */
    _addBreakpointToUI: function(breakpoint)
    {
        console.assert(!this._breakpoint(breakpoint.uiSourceCodeId, breakpoint.lineNumber));
        this._breakpoints(breakpoint.uiSourceCodeId)[breakpoint.lineNumber] = breakpoint;
        this._saveBreakpoints();
        this._breakpointAddedDelegate(breakpoint);
    },

    /**
     * @param {WebInspector.Breakpoint} breakpoint
     */
    _deleteBreakpointFromUI: function(breakpoint)
    {
        console.assert(this._breakpoint(breakpoint.uiSourceCodeId, breakpoint.lineNumber) === breakpoint);
        delete this._breakpoints(breakpoint.uiSourceCodeId)[breakpoint.lineNumber];
        this._saveBreakpoints();
        this._breakpointRemovedDelegate(breakpoint);
    },

    /**
     * @param {WebInspector.Breakpoint} breakpoint
     * @param {number} lineNumber
     */
    _moveBreakpointInUI: function(breakpoint, lineNumber)
    {
        this._deleteBreakpointFromUI(breakpoint);
        if (this._breakpoint(breakpoint.uiSourceCodeId, lineNumber))
            return false;
        breakpoint.lineNumber = lineNumber;
        this._addBreakpointToUI(breakpoint);
        return true;
    },

    /**
     * @param {string} uiSourceCodeId
     */
    _breakpoints: function(uiSourceCodeId)
    {
        if (!this._breakpointsByUILocation[uiSourceCodeId])
            this._breakpointsByUILocation[uiSourceCodeId] = {};
        return this._breakpointsByUILocation[uiSourceCodeId];
    },

    /**
     * @param {string} uiSourceCodeId
     * @param {number} lineNumber
     */
    _breakpoint: function(uiSourceCodeId, lineNumber)
    {
        return this._breakpoints(uiSourceCodeId)[lineNumber];
    },

    /**
     * @param {function(WebInspector.Breakpoint)} handler
     */
    _forEachBreakpoint: function(handler)
    {
        for (var uiSourceCodeId in this._breakpointsByUILocation) {
            var breakpoints = this._breakpointsByUILocation[uiSourceCodeId];
            for (var lineNumber in breakpoints)
                handler(breakpoints[lineNumber]);
        }
    },

    /**
     * @param {WebInspector.Breakpoint} breakpoint
     * @param {DebuggerAgent.Location} rawLocation
     */
    _setBreakpointInDebugger: function(breakpoint, rawLocation)
    {
        /**
         * @this {WebInspector.BreakpointManager}
         * @param {DebuggerAgent.BreakpointId} breakpointId
         * @param {Array.<DebuggerAgent.Location>} locations
         */
        function didSetBreakpoint(breakpointId, locations)
        {
            if (breakpoint === this._breakpoint(breakpoint.uiSourceCodeId, breakpoint.lineNumber)) {
                if (!breakpointId) {
                    this._deleteBreakpointFromUI(breakpoint);
                    return;
                }
            } else {
                if (breakpointId)
                    this._debuggerModel.removeBreakpoint(breakpointId);
                return;
            }

            this._breakpointsByDebuggerId[breakpointId] = breakpoint;
            breakpoint._debuggerId = breakpointId;
            breakpoint._debuggerLocation = locations[0];
            if (breakpoint._debuggerLocation)
                this._breakpointDebuggerLocationChanged(breakpoint);
        }
        this._debuggerModel.setBreakpointByScriptLocation(rawLocation, breakpoint.condition, didSetBreakpoint.bind(this));
    },

    /**
     * @param {WebInspector.Breakpoint} breakpoint
     */
    _removeBreakpointFromDebugger: function(breakpoint)
    {
        if (!("_debuggerId" in breakpoint))
            return;
        this._debuggerModel.removeBreakpoint(breakpoint._debuggerId);
        delete this._breakpointsByDebuggerId[breakpoint._debuggerId];
        delete breakpoint._debuggerId;
        delete breakpoint._debuggerLocation;
    },

    /**
     * @param {WebInspector.Event} event
     */
    _breakpointResolved: function(event)
    {
        var breakpoint = this._breakpointsByDebuggerId[event.data["breakpointId"]];
        breakpoint._debuggerLocation = event.data["location"];
        this._breakpointDebuggerLocationChanged(breakpoint);
    },

    _saveBreakpoints: function()
    {
        var serializedBreakpoints = [];
        /**
         * @this {WebInspector.BreakpointManager}
         * @param {WebInspector.Breakpoint} breakpoint
         */
        function serializePersistent(breakpoint)
        {
            if (breakpoint.persistent)
                serializedBreakpoints.push(breakpoint.serialize());
        }
        this._forEachBreakpoint(serializePersistent.bind(this));
        this._breakpointStorage.set(serializedBreakpoints);
    },

    reset: function()
    {
        /**
         * @this {WebInspector.BreakpointManager}
         * @param {WebInspector.Breakpoint} breakpoint
         */
        function resetBreakpoint(breakpoint)
        {
            this._removeBreakpointFromDebugger(breakpoint);
            delete breakpoint._materialized;
        }
        this._forEachBreakpoint(resetBreakpoint.bind(this));
    },

    debuggerReset: function()
    {
        /**
         * @this {WebInspector.BreakpointManager}
         * @param {WebInspector.Breakpoint} breakpoint
         */
        function resetOrDeleteBreakpoint(breakpoint)
        {
            if (breakpoint.persistent) {
                delete breakpoint.uiSourceCode;
                delete breakpoint._debuggerLocation;
            } else {
                this._deleteBreakpointFromUI(breakpoint);
                delete this._breakpointsByDebuggerId[breakpoint._debuggerId];
            }
        }
        this._forEachBreakpoint(resetOrDeleteBreakpoint.bind(this));

        for (var id in this._breakpointsByUILocation) {
            var empty = true;
            for (var lineNumber in this._breakpointsByUILocation[id]) {
                empty = false;
                break;
            }
            if (empty)
                delete this._breakpointsByUILocation[id];
        }
    }
}

/**
 * @constructor
 * @param {string} uiSourceCodeId
 * @param {number} lineNumber
 * @param {string} condition
 * @param {boolean} enabled
 * @param {boolean} persistent
 */
WebInspector.Breakpoint = function(uiSourceCodeId, lineNumber, condition, enabled, persistent)
{
    this.uiSourceCodeId = uiSourceCodeId;
    this.lineNumber = lineNumber;
    this.condition = condition;
    this.enabled = enabled;
    this.persistent = persistent;
}

WebInspector.Breakpoint.prototype = {
    /**
     * @return {Object}
     */
    serialize: function()
    {
        var serializedBreakpoint = {};
        serializedBreakpoint.sourceFileId = this.uiSourceCodeId;
        serializedBreakpoint.lineNumber = this.lineNumber;
        serializedBreakpoint.condition = this.condition;
        serializedBreakpoint.enabled = this.enabled;
        return serializedBreakpoint;
    }
}

/**
 * @param {Object} serializedBreakpoint
 * @return {WebInspector.Breakpoint}
 */
WebInspector.Breakpoint.deserialize = function(serializedBreakpoint)
{
    return new WebInspector.Breakpoint(
            serializedBreakpoint.sourceFileId,
            serializedBreakpoint.lineNumber,
            serializedBreakpoint.condition,
            serializedBreakpoint.enabled,
            true);
}
