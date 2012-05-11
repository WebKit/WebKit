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
 * @extends {WebInspector.Object}
 * @param {WebInspector.Setting} breakpointStorage
 * @param {WebInspector.DebuggerModel} debuggerModel
 */
WebInspector.BreakpointManager = function(breakpointStorage, debuggerModel)
{
    this._storage = new WebInspector.BreakpointManager.Storage(this, breakpointStorage);
    this._debuggerModel = debuggerModel;

    this._breakpoints = [];
    this._breakpointForDebuggerId = {};
    this._breakpointsForUILocation = {};
    this._sourceFilesWithRestoredBreakpoints = {};

    this._debuggerModel.addEventListener(WebInspector.DebuggerModel.Events.BreakpointResolved, this._breakpointResolved, this);
    this._debuggerModel.addEventListener(WebInspector.DebuggerModel.Events.GlobalObjectCleared, this._debuggerReset, this);
}

WebInspector.BreakpointManager.Events = {
    BreakpointAdded: "breakpoint-added",
    BreakpointRemoved: "breakpoint-removed"
}

WebInspector.BreakpointManager.prototype = {
    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     */
    restoreBreakpoints: function(uiSourceCode)
    {
        var sourceFileId = uiSourceCode.breakpointStorageId();
        if (!sourceFileId || this._sourceFilesWithRestoredBreakpoints[sourceFileId])
            return;
        this._sourceFilesWithRestoredBreakpoints[sourceFileId] = true;

        // Erase provisional breakpoints prior to restoring them.
        for (var debuggerId in this._breakpointForDebuggerId) {
            var breakpoint = this._breakpointForDebuggerId[debuggerId];
            if (breakpoint._sourceCodeStorageId !== sourceFileId)
                continue;
            this._debuggerModel.removeBreakpoint(debuggerId);
            delete this._breakpointForDebuggerId[debuggerId];
        }
        this._storage._restoreBreakpoints(uiSourceCode);
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     * @param {number} lineNumber
     * @param {string} condition
     * @param {boolean} enabled
     * @return {WebInspector.BreakpointManager.Breakpoint}
     */
    setBreakpoint: function(uiSourceCode, lineNumber, condition, enabled)
    {
        this._debuggerModel.setBreakpointsActive(true);
        return this._innerSetBreakpoint(uiSourceCode, lineNumber, condition, enabled);
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     * @param {number} lineNumber
     * @param {string} condition
     * @param {boolean} enabled
     * @return {WebInspector.BreakpointManager.Breakpoint}
     */
    _innerSetBreakpoint: function(uiSourceCode, lineNumber, condition, enabled)
    {
        var breakpoint = this.findBreakpoint(uiSourceCode, lineNumber);
        if (breakpoint) {
            breakpoint._updateBreakpoint(condition, enabled);
            return breakpoint;
        }
        breakpoint = new WebInspector.BreakpointManager.Breakpoint(this, uiSourceCode, lineNumber, condition, enabled);
        this._breakpoints.push(breakpoint);
        return breakpoint;
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     * @param {number} lineNumber
     * @return {?WebInspector.BreakpointManager.Breakpoint}
     */
    findBreakpoint: function(uiSourceCode, lineNumber)
    {
        var breakpoints = this._breakpointsForUILocation[uiSourceCode.id + ":" + lineNumber];
        return breakpoints ? breakpoints[0] : null;
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     * @return {Array.<Object>}
     */
    breakpointLocationsForUISourceCode: function(uiSourceCode)
    {
        var result = [];
        for (var i = 0; i < this._breakpoints.length; ++i) {
            var breakpoint = this._breakpoints[i];
            var uiLocations = breakpoint._uiLocations.values();
            for (var j = 0; j < uiLocations.length; ++j) {
                var uiLocation = uiLocations[j];
                if (uiLocation.uiSourceCode === uiSourceCode)
                    result.push({breakpoint: breakpoint, uiLocation: uiLocation});
            }
        }
        return result;
    },

    removeAllBreakpoints: function()
    {
        var breakpoints = this._breakpoints.slice();
        for (var i = 0; i < breakpoints.length; ++i)
            breakpoints[i].remove();
    },

    reset: function()
    {
        // Remove all breakpoints from UI and debugger, do not update storage.
        this._storage._muted = true;
        this.removeAllBreakpoints();
        delete this._storage._muted;

        // Remove all provisional breakpoints from the debugger.
        for (var debuggerId in this._breakpointForDebuggerId)
            this._debuggerModel.removeBreakpoint(debuggerId);
        this._breakpointForDebuggerId = {};
        this._sourceFilesWithRestoredBreakpoints = {};
    },

    _debuggerReset: function()
    {
        var breakpoints = this._breakpoints.slice();
        for (var i = 0; i < breakpoints.length; ++i) {
            breakpoints[i]._resetLocations();
            breakpoints[i]._isProvisional = true;
        }
        this._breakpoints = [];
        this._breakpointsForUILocation = {};
        this._sourceFilesWithRestoredBreakpoints = {};
    },

    _breakpointResolved: function(event)
    {
        var breakpointId = /** @type {DebuggerAgent.BreakpointId} */ event.data.breakpointId;
        var location = /** @type {DebuggerAgent.Location} */ event.data.location;
        var breakpoint = this._breakpointForDebuggerId[breakpointId];
        if (!breakpoint || breakpoint._isProvisional)
            return;
        breakpoint._addResolvedLocation(location);
    },

    /**
     * @param {WebInspector.BreakpointManager.Breakpoint} breakpoint
     * @param {boolean} removeFromStorage
     */
    _removeBreakpoint: function(breakpoint, removeFromStorage)
    {
        console.assert(!breakpoint._debuggerId)
        this._breakpoints.remove(breakpoint);
        if (removeFromStorage)
            this._storage._removeBreakpoint(breakpoint);
    },

    /**
     * @param {WebInspector.BreakpointManager.Breakpoint} breakpoint
     * @param {WebInspector.UILocation} uiLocation
     */
    _uiLocationAdded: function(breakpoint, uiLocation)
    {
        var key = uiLocation.uiSourceCode.id + ":" + uiLocation.lineNumber;
        var breakpoints = this._breakpointsForUILocation[key];
        if (!breakpoints) {
            breakpoints = [];
            this._breakpointsForUILocation[key] = breakpoints;
        }
        breakpoints.push(breakpoint);
        this.dispatchEventToListeners(WebInspector.BreakpointManager.Events.BreakpointAdded, {breakpoint: breakpoint, uiLocation: uiLocation});
    },

    /**
     * @param {WebInspector.BreakpointManager.Breakpoint} breakpoint
     * @param {WebInspector.UILocation} uiLocation
     */
    _uiLocationRemoved: function(breakpoint, uiLocation)
    {
        var key = uiLocation.uiSourceCode.id + ":" + uiLocation.lineNumber;
        var breakpoints = this._breakpointsForUILocation[key];
        if (!breakpoints)
            return;
        breakpoints.remove(breakpoint);
        if (!breakpoints.length)
            delete this._breakpointsForUILocation[key];
        this.dispatchEventToListeners(WebInspector.BreakpointManager.Events.BreakpointRemoved, {breakpoint: breakpoint, uiLocation: uiLocation});
    }
}

WebInspector.BreakpointManager.prototype.__proto__ = WebInspector.Object.prototype;

/**
 * @constructor
 * @param {WebInspector.BreakpointManager} breakpointManager
 * @param {WebInspector.UISourceCode} uiSourceCode
 * @param {number} lineNumber
 * @param {string} condition
 * @param {boolean} enabled
 */
WebInspector.BreakpointManager.Breakpoint = function(breakpointManager, uiSourceCode, lineNumber, condition, enabled)
{
    this._breakpointManager = breakpointManager;
    this._primaryUILocation = new WebInspector.UILocation(uiSourceCode, lineNumber, 0);
    this._sourceCodeStorageId = uiSourceCode.breakpointStorageId();
    this._liveLocations = [];
    this._uiLocations = new Map();

    // Force breakpoint update.
    /** @type {string} */ this._condition;
    /** @type {boolean} */ this._enabled;
    this._updateBreakpoint(condition, enabled);
}

WebInspector.BreakpointManager.Breakpoint.prototype = {
    /**
     * @return {WebInspector.UILocation}
     */
    primaryUILocation: function()
    {
        return this._primaryUILocation;
    },

    /**
     * @param {DebuggerAgent.Location} location
     */
    _addResolvedLocation: function(location)
    {
        this._liveLocations.push(this._breakpointManager._debuggerModel.createLiveLocation(location, this._locationUpdated.bind(this, location)));
    },

    /**
     * @param {DebuggerAgent.Location} location
     * @param {WebInspector.UILocation} uiLocation
     */
    _locationUpdated: function(location, uiLocation)
    {
        var oldUILocation = /** @type {WebInspector.UILocation} */ this._uiLocations.get(location);
        if (oldUILocation)
            this._breakpointManager._uiLocationRemoved(this, oldUILocation);
        this._uiLocations.put(location, uiLocation);
        this._breakpointManager._uiLocationAdded(this, uiLocation);
    },

    /**
     * @return {boolean}
     */
    enabled: function()
    {
        return this._enabled;
    },

    /**
     * @param {boolean} enabled
     */
    setEnabled: function(enabled)
    {
        this._updateBreakpoint(this._condition, enabled);
    },

    /**
     * @return {string}
     */
    condition: function()
    {
        return this._condition;
    },

    /**
     * @param {string} condition
     */
    setCondition: function(condition)
    {
        this._updateBreakpoint(condition, this._enabled);
    },

    /**
     * @param {string} condition
     * @param {boolean} enabled
     */
    _updateBreakpoint: function(condition, enabled)
    {
        if (this._enabled === enabled && this._condition === condition)
            return;

        if (this._enabled)
            this._removeFromDebugger();

        this._enabled = enabled;
        this._condition = condition;
        this._breakpointManager._storage._updateBreakpoint(this);

        if (this._enabled) {
            this._setInDebugger();
            return;
        }

        this._fakeBreakpointAtPrimaryLocation();
    },

    remove: function()
    {
        this._resetLocations();
        this._removeFromDebugger();
        this._breakpointManager._removeBreakpoint(this, true);
    },

    _setInDebugger: function()
    {
        var rawLocation = this._primaryUILocation.uiLocationToRawLocation();
        this._breakpointManager._debuggerModel.setBreakpointByScriptLocation(rawLocation, this._condition, didSetBreakpoint.bind(this));
        /**
         * @this {WebInspector.BreakpointManager.Breakpoint}
         * @param {?DebuggerAgent.BreakpointId} breakpointId
         * @param {Array.<DebuggerAgent.Location>} locations
         */
        function didSetBreakpoint(breakpointId, locations)
        {
            if (!breakpointId) {
                this._resetLocations();
                this._breakpointManager._removeBreakpoint(this, false);
                return;
            }

            this._debuggerId = breakpointId;
            this._breakpointManager._breakpointForDebuggerId[breakpointId] = this;

            if (!locations.length) {
                this._fakeBreakpointAtPrimaryLocation();
                return;
            }

            this._resetLocations();
            for (var i = 0; i < locations.length; ++i) {
                var script = this._breakpointManager._debuggerModel.scriptForId(locations[i].scriptId);
                var uiLocation = script.rawLocationToUILocation(locations[i]);
                if (this._breakpointManager.findBreakpoint(uiLocation.uiSourceCode, uiLocation.lineNumber)) {
                    // location clash
                    this.remove();
                    return;
                }
            }

            for (var i = 0; i < locations.length; ++i)
                this._addResolvedLocation(locations[i]);
        }
    },

    _removeFromDebugger: function()
    {
        if (this._debuggerId) {
            this._breakpointManager._debuggerModel.removeBreakpoint(this._debuggerId);
            delete this._breakpointManager._breakpointForDebuggerId[this._debuggerId];
            delete this._debuggerId;
        }
    },

    _resetLocations: function()
    {
        var uiLocations = this._uiLocations.values();
        for (var i = 0; i < uiLocations.length; ++i)
            this._breakpointManager._uiLocationRemoved(this, uiLocations[i]);

        for (var i = 0; i < this._liveLocations.length; ++i)
            this._liveLocations[i].dispose();
        this._liveLocations = [];

        this._uiLocations = new Map();
    },

    /**
     * @return {string}
     */
    _breakpointStorageId: function()
    {
        return this._primaryUILocation.uiSourceCode.breakpointStorageId() + ":" + this._primaryUILocation.lineNumber;
    },

    _fakeBreakpointAtPrimaryLocation: function()
    {
        this._resetLocations();
        this._uiLocations.put({}, this._primaryUILocation);
        this._breakpointManager._uiLocationAdded(this, this._primaryUILocation);
    }
}

/**
 * @constructor
 * @param {WebInspector.BreakpointManager} breakpointManager
 * @param {WebInspector.Setting} setting
 */
WebInspector.BreakpointManager.Storage = function(breakpointManager, setting)
{
    this._breakpointManager = breakpointManager;
    this._setting = setting;
    var breakpoints = this._setting.get();
    /** @type {Object.<string,WebInspector.BreakpointManager.Storage.Item>} */
    this._breakpoints = {};
    for (var i = 0; i < breakpoints.length; ++i) {
        var breakpoint = /** @type {WebInspector.BreakpointManager.Storage.Item} */ breakpoints[i];
        this._breakpoints[breakpoint.sourceFileId + ":" + breakpoint.lineNumber] = breakpoint;
    }
}

WebInspector.BreakpointManager.Storage.prototype = {
    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     */
    _restoreBreakpoints: function(uiSourceCode)
    {
        this._muted = true;
        var breakpointStorageId = uiSourceCode.breakpointStorageId();
        for (var id in this._breakpoints) {
            var breakpoint = this._breakpoints[id];
            if (breakpoint.sourceFileId === breakpointStorageId)
                this._breakpointManager._innerSetBreakpoint(uiSourceCode, breakpoint.lineNumber, breakpoint.condition, breakpoint.enabled);
        }
        delete this._muted;
    },

    /**
     * @param {WebInspector.BreakpointManager.Breakpoint} breakpoint
     */
    _updateBreakpoint: function(breakpoint)
    {
        if (this._muted)
            return;
        this._breakpoints[breakpoint._breakpointStorageId()] = new WebInspector.BreakpointManager.Storage.Item(breakpoint);
        this._save();
    },

    /**
     * @param {WebInspector.BreakpointManager.Breakpoint} breakpoint
     */
    _removeBreakpoint: function(breakpoint)
    {
        if (this._muted)
            return;
        delete this._breakpoints[breakpoint._breakpointStorageId()];
        this._save();
    },

    _save: function()
    {
        var breakpointsArray = [];
        for (var id in this._breakpoints)
            breakpointsArray.push(this._breakpoints[id]);
        this._setting.set(breakpointsArray);
    }
}

/**
 * @constructor
 * @param {WebInspector.BreakpointManager.Breakpoint} breakpoint
 */
WebInspector.BreakpointManager.Storage.Item = function(breakpoint)
{
    var primaryUILocation = breakpoint.primaryUILocation();
    this.sourceFileId = primaryUILocation.uiSourceCode.breakpointStorageId();
    this.lineNumber = primaryUILocation.lineNumber;
    this.condition = breakpoint.condition();
    this.enabled = breakpoint.enabled();
}
