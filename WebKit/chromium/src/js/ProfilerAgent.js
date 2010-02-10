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
 * FIXME: change field naming style to use trailing underscore.
 * @constructor
 */
devtools.ProfilerAgent = function()
{
    RemoteProfilerAgent.didGetActiveProfilerModules = this.didGetActiveProfilerModules_.bind(this);
    RemoteProfilerAgent.didGetLogLines = this.didGetLogLines_.bind(this);

    /**
     * Active profiler modules flags.
     * @type {number}
     */
    this.activeProfilerModules_ = devtools.ProfilerAgent.ProfilerModules.PROFILER_MODULE_NONE;

    /**
     * Interval for polling profiler state.
     * @type {number}
     */
    this.getActiveProfilerModulesInterval_ = null;

    /**
     * Profiler log position.
     * @type {number}
     */
    this.logPosition_ = 0;

    /**
     * Whether log contents retrieval must be forced next time.
     * @type {boolean}
     */
    this.forceGetLogLines_ = false;

    /**
     * Profiler processor instance.
     * @type {devtools.profiler.Processor}
     */
    this.profilerProcessor_ = new devtools.profiler.Processor();
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

    this.profilerProcessor_.setCallbacks(
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
    this.forceGetLogLines_ = true;
    this.getActiveProfilerModulesInterval_ = setInterval(function() { RemoteProfilerAgent.getActiveProfilerModules(); }, 1000);
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
        var pos = this.logPosition_;
        // Active modules will not change, instead, a snapshot will be logged.
        setTimeout(function() { RemoteProfilerAgent.getLogLines(pos); }, 500);
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
devtools.ProfilerAgent.prototype.didGetActiveProfilerModules_ = function(modules)
{
    var profModules = devtools.ProfilerAgent.ProfilerModules;
    var profModuleNone = profModules.PROFILER_MODULE_NONE;
    if (this.forceGetLogLines_ || (modules !== profModuleNone && this.activeProfilerModules_ === profModuleNone)) {
        this.forceGetLogLines_ = false;
        // Start to query log data.
        RemoteProfilerAgent.getLogLines(this.logPosition_);
    }
    this.activeProfilerModules_ = modules;
    // Update buttons.
    WebInspector.setRecordingProfile(modules & profModules.PROFILER_MODULE_CPU);
};


/**
 * Handles a portion of a profiler log retrieved by getLogLines call.
 * @param {number} pos Current position in log.
 * @param {string} log A portion of profiler log.
 */
devtools.ProfilerAgent.prototype.didGetLogLines_ = function(pos, log)
{
    this.logPosition_ = pos;
    if (log.length > 0)
        this.profilerProcessor_.processLogChunk(log);
    else if (this.activeProfilerModules_ === devtools.ProfilerAgent.ProfilerModules.PROFILER_MODULE_NONE) {
        // No new data and profiling is stopped---suspend log reading.
        return;
    }
    setTimeout(function() { RemoteProfilerAgent.getLogLines(pos); }, 500);
};
