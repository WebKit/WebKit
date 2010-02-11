/*
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

/**
 * @fileoverview Provides communication interface to remote v8 profiler.
 */

/**
 * @constructor
 */
devtools.ProfilerAgent = function()
{
    RemoteProfilerAgent.didGetActiveProfilerModules = this._didGetActiveProfilerModules.bind(this);
    RemoteProfilerAgent.didGetLogLines = this._didGetLogLines.bind(this);

    /**
     * Active profiler modules flags.
     * @type {number}
     */
    this._activeProfilerModules = devtools.ProfilerAgent.ProfilerModules.PROFILER_MODULE_NONE;

    /**
     * Interval for polling profiler state.
     * @type {number}
     */
    this._getActiveProfilerModulesInterval = null;

    /**
     * Profiler log position.
     * @type {number}
     */
    this._logPosition = 0;

    /**
     * Last requested log position.
     * @type {number}
     */
    this._lastRequestedLogPosition = -1;

    /**
     * Whether log contents retrieval must be forced next time.
     * @type {boolean}
     */
    this._forceGetLogLines = false;

    /**
     * Profiler processor instance.
     * @type {devtools.profiler.Processor}
     */
    this._profilerProcessor = new devtools.profiler.Processor();
};


/**
 * A copy of enum from include/v8.h
 * @enum {number}
 */
devtools.ProfilerAgent.ProfilerModules = {
    PROFILER_MODULE_NONE: 0,
    PROFILER_MODULE_CPU: 1,
    PROFILER_MODULE_HEAP_STATS: 1 << 1,
    PROFILER_MODULE_JS_CONSTRUCTORS: 1 << 2,
    PROFILER_MODULE_HEAP_SNAPSHOT: 1 << 16
};


/**
 * Sets up callbacks that deal with profiles processing.
 */
devtools.ProfilerAgent.prototype.setupProfilerProcessorCallbacks = function()
{
    // A temporary icon indicating that the profile is being processed.
    var processingIcon = new WebInspector.SidebarTreeElement(
        "profile-sidebar-tree-item",
        WebInspector.UIString("Processing..."),
        '', null, false);
    var profilesSidebar = WebInspector.panels.profiles.getProfileType(WebInspector.CPUProfileType.TypeId).treeElement;

    this._profilerProcessor.setCallbacks(
        function onProfileProcessingStarted() {
            // Set visually empty string. Subtitle hiding is done via styles
            // manipulation which doesn't play well with dynamic append / removal.
            processingIcon.subtitle = " ";
            profilesSidebar.appendChild(processingIcon);
        },
        function onProfileProcessingStatus(ticksCount) {
            processingIcon.subtitle = WebInspector.UIString("%d ticks processed", ticksCount);
        },
        function onProfileProcessingFinished(profile) {
            profilesSidebar.removeChild(processingIcon);
            profile.typeId = WebInspector.CPUProfileType.TypeId;
            InspectorBackend.addFullProfile(profile);
            WebInspector.addProfileHeader(profile);
            // If no profile is currently shown, show the new one.
            var profilesPanel = WebInspector.panels.profiles;
            if (!profilesPanel.visibleView) {
                profilesPanel.showProfile(profile);
            }
        }
    );
};


/**
 * Initializes profiling state.
 */
devtools.ProfilerAgent.prototype.initializeProfiling = function()
{
    this.setupProfilerProcessorCallbacks();
    this._forceGetLogLines = true;
    this._getActiveProfilerModulesInterval = setInterval(function() { RemoteProfilerAgent.getActiveProfilerModules(); }, 1000);
};


/**
 * Requests the next chunk of log lines.
 * @param {boolean} immediately Do not postpone the request.
 * @private
 */
devtools.ProfilerAgent.prototype._getNextLogLines = function(immediately)
{
    if (this._lastRequestedLogPosition == this._logPosition)
        return;
    var pos = this._lastRequestedLogPosition = this._logPosition;
    if (immediately)
        RemoteProfilerAgent.getLogLines(pos);
    else
        setTimeout(function() { RemoteProfilerAgent.getLogLines(pos); }, 500);
};


/**
 * Starts profiling.
 * @param {number} modules List of modules to enable.
 */
devtools.ProfilerAgent.prototype.startProfiling = function(modules)
{
    var cmd = new devtools.DebugCommand("profile", {
        "modules": modules,
        "command": "resume"});
    devtools.DebuggerAgent.sendCommand_(cmd);
    RemoteDebuggerAgent.processDebugCommands();
    if (modules & devtools.ProfilerAgent.ProfilerModules.PROFILER_MODULE_HEAP_SNAPSHOT) {
        // Active modules will not change, instead, a snapshot will be logged.
        this._getNextLogLines();
    }
};


/**
 * Stops profiling.
 */
devtools.ProfilerAgent.prototype.stopProfiling = function(modules)
{
    var cmd = new devtools.DebugCommand("profile", {
        "modules": modules,
        "command": "pause"});
    devtools.DebuggerAgent.sendCommand_(cmd);
    RemoteDebuggerAgent.processDebugCommands();
};


/**
 * Handles current profiler status.
 * @param {number} modules List of active (started) modules.
 */
devtools.ProfilerAgent.prototype._didGetActiveProfilerModules = function(modules)
{
    var profModules = devtools.ProfilerAgent.ProfilerModules;
    var profModuleNone = profModules.PROFILER_MODULE_NONE;
    if (this._forceGetLogLines || (modules !== profModuleNone && this._activeProfilerModules === profModuleNone)) {
        this._forceGetLogLines = false;
        // Start to query log data.
        this._getNextLogLines(true);
    }
    this._activeProfilerModules = modules;
    // Update buttons.
    WebInspector.setRecordingProfile(modules & profModules.PROFILER_MODULE_CPU);
};


/**
 * Handles a portion of a profiler log retrieved by getLogLines call.
 * @param {number} pos Current position in log.
 * @param {string} log A portion of profiler log.
 */
devtools.ProfilerAgent.prototype._didGetLogLines = function(pos, log)
{
    this._logPosition = pos;
    if (log.length > 0)
        this._profilerProcessor.processLogChunk(log);
    else {
        // Allow re-reading from the last position.
        this._lastRequestedLogPosition = this._logPosition - 1;
        if (this._activeProfilerModules === devtools.ProfilerAgent.ProfilerModules.PROFILER_MODULE_NONE)
            // No new data and profiling is stopped---suspend log reading.
            return;
    }
    this._getNextLogLines();
};
