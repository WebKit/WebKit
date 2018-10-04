/*
 * Copyright (C) 2013-2016 Apple Inc. All rights reserved.
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

WI.DebuggerManager = class DebuggerManager extends WI.Object
{
    constructor()
    {
        super();

        DebuggerAgent.enable();

        WI.notifications.addEventListener(WI.Notification.DebugUIEnabledDidChange, this._debugUIEnabledDidChange, this);

        WI.Breakpoint.addEventListener(WI.Breakpoint.Event.DisplayLocationDidChange, this._breakpointDisplayLocationDidChange, this);
        WI.Breakpoint.addEventListener(WI.Breakpoint.Event.DisabledStateDidChange, this._breakpointDisabledStateDidChange, this);
        WI.Breakpoint.addEventListener(WI.Breakpoint.Event.ConditionDidChange, this._breakpointEditablePropertyDidChange, this);
        WI.Breakpoint.addEventListener(WI.Breakpoint.Event.IgnoreCountDidChange, this._breakpointEditablePropertyDidChange, this);
        WI.Breakpoint.addEventListener(WI.Breakpoint.Event.AutoContinueDidChange, this._breakpointEditablePropertyDidChange, this);
        WI.Breakpoint.addEventListener(WI.Breakpoint.Event.ActionsDidChange, this._handleBreakpointActionsDidChange, this);

        WI.timelineManager.addEventListener(WI.TimelineManager.Event.CapturingWillStart, this._timelineCapturingWillStart, this);
        WI.timelineManager.addEventListener(WI.TimelineManager.Event.CapturingStopped, this._timelineCapturingStopped, this);

        WI.targetManager.addEventListener(WI.TargetManager.Event.TargetRemoved, this._targetRemoved, this);

        WI.Frame.addEventListener(WI.Frame.Event.MainResourceDidChange, this._mainResourceDidChange, this);

        this._breakpointsSetting = new WI.Setting("breakpoints", []);
        this._breakpointsEnabledSetting = new WI.Setting("breakpoints-enabled", true);
        this._allExceptionsBreakpointEnabledSetting = new WI.Setting("break-on-all-exceptions", false);
        this._uncaughtExceptionsBreakpointEnabledSetting = new WI.Setting("break-on-uncaught-exceptions", false);
        this._assertionFailuresBreakpointEnabledSetting = new WI.Setting("break-on-assertion-failures", false);
        this._asyncStackTraceDepthSetting = new WI.Setting("async-stack-trace-depth", 200);

        let specialBreakpointLocation = new WI.SourceCodeLocation(null, Infinity, Infinity);

        this._allExceptionsBreakpoint = new WI.Breakpoint(specialBreakpointLocation, !this._allExceptionsBreakpointEnabledSetting.value);
        this._allExceptionsBreakpoint.resolved = true;

        this._uncaughtExceptionsBreakpoint = new WI.Breakpoint(specialBreakpointLocation, !this._uncaughtExceptionsBreakpointEnabledSetting.value);

        this._assertionFailuresBreakpoint = new WI.Breakpoint(specialBreakpointLocation, !this._assertionFailuresBreakpointEnabledSetting.value);
        this._assertionFailuresBreakpoint.resolved = true;

        this._breakpoints = [];
        this._breakpointContentIdentifierMap = new Map;
        this._breakpointScriptIdentifierMap = new Map;
        this._breakpointIdMap = new Map;

        this._breakOnExceptionsState = "none";
        this._updateBreakOnExceptionsState();

        this._nextBreakpointActionIdentifier = 1;

        this._activeCallFrame = null;

        this._internalWebKitScripts = [];
        this._targetDebuggerDataMap = new Map;
        this._targetDebuggerDataMap.set(WI.mainTarget, new WI.DebuggerData(WI.mainTarget));

        // Used to detect deleted probe actions.
        this._knownProbeIdentifiersForBreakpoint = new Map;

        // Main lookup tables for probes and probe sets.
        this._probesByIdentifier = new Map;
        this._probeSetsByBreakpoint = new Map;

        // Restore the correct breakpoints enabled setting if Web Inspector had
        // previously been left in a state where breakpoints were temporarily disabled.
        this._temporarilyDisabledBreakpointsRestoreSetting = new WI.Setting("temporarily-disabled-breakpoints-restore", null);
        if (this._temporarilyDisabledBreakpointsRestoreSetting.value !== null) {
            this._breakpointsEnabledSetting.value = this._temporarilyDisabledBreakpointsRestoreSetting.value;
            this._temporarilyDisabledBreakpointsRestoreSetting.value = null;
        }

        DebuggerAgent.setBreakpointsActive(this._breakpointsEnabledSetting.value);
        DebuggerAgent.setPauseOnExceptions(this._breakOnExceptionsState);

        // COMPATIBILITY (iOS 10): DebuggerAgent.setPauseOnAssertions did not exist yet.
        if (DebuggerAgent.setPauseOnAssertions)
            DebuggerAgent.setPauseOnAssertions(this._assertionFailuresBreakpointEnabledSetting.value);

        // COMPATIBILITY (iOS 10): Debugger.setAsyncStackTraceDepth did not exist yet.
        if (DebuggerAgent.setAsyncStackTraceDepth)
            DebuggerAgent.setAsyncStackTraceDepth(this._asyncStackTraceDepthSetting.value);

        // COMPATIBILITY (iOS 12): DebuggerAgent.setPauseForInternalScripts did not exist yet.
        if (DebuggerAgent.setPauseForInternalScripts) {
            let updateBackendSetting = () => { DebuggerAgent.setPauseForInternalScripts(WI.settings.pauseForInternalScripts.value); };
            WI.settings.pauseForInternalScripts.addEventListener(WI.Setting.Event.Changed, updateBackendSetting);

            updateBackendSetting();
        }

        this._ignoreBreakpointDisplayLocationDidChangeEvent = false;

        function restoreBreakpointsSoon() {
            this._restoringBreakpoints = true;
            for (let cookie of this._breakpointsSetting.value)
                this.addBreakpoint(new WI.Breakpoint(cookie));
            this._restoringBreakpoints = false;
        }

        // Ensure that all managers learn about restored breakpoints,
        // regardless of their initialization order.
        setTimeout(restoreBreakpointsSoon.bind(this), 0);
    }

    // Public

    get paused()
    {
        for (let [target, targetData] of this._targetDebuggerDataMap) {
            if (targetData.paused)
                return true;
        }

        return false;
    }

    get activeCallFrame()
    {
        return this._activeCallFrame;
    }

    set activeCallFrame(callFrame)
    {
        if (callFrame === this._activeCallFrame)
            return;

        this._activeCallFrame = callFrame || null;

        this.dispatchEventToListeners(WI.DebuggerManager.Event.ActiveCallFrameDidChange);
    }

    dataForTarget(target)
    {
        let targetData = this._targetDebuggerDataMap.get(target);
        if (targetData)
            return targetData;

        targetData = new WI.DebuggerData(target);
        this._targetDebuggerDataMap.set(target, targetData);
        return targetData;
    }

    get allExceptionsBreakpoint() { return this._allExceptionsBreakpoint; }
    get uncaughtExceptionsBreakpoint() { return this._uncaughtExceptionsBreakpoint; }
    get assertionFailuresBreakpoint() { return this._assertionFailuresBreakpoint; }
    get breakpoints() { return this._breakpoints; }

    breakpointForIdentifier(id)
    {
        return this._breakpointIdMap.get(id) || null;
    }

    breakpointsForSourceCode(sourceCode)
    {
        console.assert(sourceCode instanceof WI.Resource || sourceCode instanceof WI.Script);

        if (sourceCode instanceof WI.SourceMapResource) {
            let originalSourceCodeBreakpoints = this.breakpointsForSourceCode(sourceCode.sourceMap.originalSourceCode);
            return originalSourceCodeBreakpoints.filter(function(breakpoint) {
                return breakpoint.sourceCodeLocation.displaySourceCode === sourceCode;
            });
        }

        let contentIdentifierBreakpoints = this._breakpointContentIdentifierMap.get(sourceCode.contentIdentifier);
        if (contentIdentifierBreakpoints) {
            this._associateBreakpointsWithSourceCode(contentIdentifierBreakpoints, sourceCode);
            return contentIdentifierBreakpoints;
        }

        if (sourceCode instanceof WI.Script) {
            let scriptIdentifierBreakpoints = this._breakpointScriptIdentifierMap.get(sourceCode.id);
            if (scriptIdentifierBreakpoints) {
                this._associateBreakpointsWithSourceCode(scriptIdentifierBreakpoints, sourceCode);
                return scriptIdentifierBreakpoints;
            }
        }

        return [];
    }

    breakpointForSourceCodeLocation(sourceCodeLocation)
    {
        console.assert(sourceCodeLocation instanceof WI.SourceCodeLocation);

        for (let breakpoint of this.breakpointsForSourceCode(sourceCodeLocation.sourceCode)) {
            if (breakpoint.sourceCodeLocation.isEqual(sourceCodeLocation))
                return breakpoint;
        }

        return null;
    }

    isBreakpointRemovable(breakpoint)
    {
        return breakpoint !== this._allExceptionsBreakpoint
            && breakpoint !== this._uncaughtExceptionsBreakpoint;
    }

    isBreakpointSpecial(breakpoint)
    {
        return !this.isBreakpointRemovable(breakpoint)
            || breakpoint === this._assertionFailuresBreakpoint;
    }

    isBreakpointEditable(breakpoint)
    {
        return !this.isBreakpointSpecial(breakpoint);
    }

    get breakpointsEnabled()
    {
        return this._breakpointsEnabledSetting.value;
    }

    set breakpointsEnabled(enabled)
    {
        if (this._breakpointsEnabledSetting.value === enabled)
            return;

        console.assert(!(enabled && this.breakpointsDisabledTemporarily), "Should not enable breakpoints when we are temporarily disabling breakpoints.");
        if (enabled && this.breakpointsDisabledTemporarily)
            return;

        this._breakpointsEnabledSetting.value = enabled;

        this._updateBreakOnExceptionsState();

        for (let target of WI.targets) {
            target.DebuggerAgent.setBreakpointsActive(enabled);
            target.DebuggerAgent.setPauseOnExceptions(this._breakOnExceptionsState);
        }

        this.dispatchEventToListeners(WI.DebuggerManager.Event.BreakpointsEnabledDidChange);
    }

    get breakpointsDisabledTemporarily()
    {
        return this._temporarilyDisabledBreakpointsRestoreSetting.value !== null;
    }

    scriptForIdentifier(id, target)
    {
        console.assert(target instanceof WI.Target);
        return this.dataForTarget(target).scriptForIdentifier(id);
    }

    scriptsForURL(url, target)
    {
        // FIXME: This may not be safe. A Resource's URL may differ from a Script's URL.
        console.assert(target instanceof WI.Target);
        return this.dataForTarget(target).scriptsForURL(url);
    }

    get searchableScripts()
    {
        return this.knownNonResourceScripts.filter((script) => !!script.contentIdentifier);
    }

    get knownNonResourceScripts()
    {
        let knownScripts = [];

        for (let [target, targetData] of this._targetDebuggerDataMap) {
            for (let script of targetData.scripts) {
                if (script.resource)
                    continue;
                if (isWebInspectorConsoleEvaluationScript(script.sourceURL))
                    continue;
                if (!WI.isDebugUIEnabled() && isWebKitInternalScript(script.sourceURL))
                    continue;
                knownScripts.push(script);
            }
        }

        return knownScripts;
    }

    get asyncStackTraceDepth()
    {
        return this._asyncStackTraceDepthSetting.value;
    }

    set asyncStackTraceDepth(x)
    {
        if (this._asyncStackTraceDepthSetting.value === x)
            return;

        this._asyncStackTraceDepthSetting.value = x;

        for (let target of WI.targets)
            target.DebuggerAgent.setAsyncStackTraceDepth(this._asyncStackTraceDepthSetting.value);
    }

    get probeSets()
    {
        return [...this._probeSetsByBreakpoint.values()];
    }

    probeForIdentifier(identifier)
    {
        return this._probesByIdentifier.get(identifier);
    }

    pause()
    {
        if (this.paused)
            return Promise.resolve();

        this.dispatchEventToListeners(WI.DebuggerManager.Event.WaitingToPause);

        let listener = new WI.EventListener(this, true);

        let managerResult = new Promise(function(resolve, reject) {
            listener.connect(WI.debuggerManager, WI.DebuggerManager.Event.Paused, resolve);
        });

        let promises = [];
        for (let [target, targetData] of this._targetDebuggerDataMap)
            promises.push(targetData.pauseIfNeeded());

        return Promise.all([managerResult, ...promises]);
    }

    resume()
    {
        if (!this.paused)
            return Promise.resolve();

        let listener = new WI.EventListener(this, true);

        let managerResult = new Promise(function(resolve, reject) {
            listener.connect(WI.debuggerManager, WI.DebuggerManager.Event.Resumed, resolve);
        });

        let promises = [];
        for (let [target, targetData] of this._targetDebuggerDataMap)
            promises.push(targetData.resumeIfNeeded());

        return Promise.all([managerResult, ...promises]);
    }

    stepOver()
    {
        if (!this.paused)
            return Promise.reject(new Error("Cannot step over because debugger is not paused."));

        let listener = new WI.EventListener(this, true);

        let managerResult = new Promise(function(resolve, reject) {
            listener.connect(WI.debuggerManager, WI.DebuggerManager.Event.ActiveCallFrameDidChange, resolve);
        });

        let protocolResult = this._activeCallFrame.target.DebuggerAgent.stepOver()
            .catch(function(error) {
                listener.disconnect();
                console.error("DebuggerManager.stepOver failed: ", error);
                throw error;
            });

        return Promise.all([managerResult, protocolResult]);
    }

    stepInto()
    {
        if (!this.paused)
            return Promise.reject(new Error("Cannot step into because debugger is not paused."));

        let listener = new WI.EventListener(this, true);

        let managerResult = new Promise(function(resolve, reject) {
            listener.connect(WI.debuggerManager, WI.DebuggerManager.Event.ActiveCallFrameDidChange, resolve);
        });

        let protocolResult = this._activeCallFrame.target.DebuggerAgent.stepInto()
            .catch(function(error) {
                listener.disconnect();
                console.error("DebuggerManager.stepInto failed: ", error);
                throw error;
            });

        return Promise.all([managerResult, protocolResult]);
    }

    stepOut()
    {
        if (!this.paused)
            return Promise.reject(new Error("Cannot step out because debugger is not paused."));

        let listener = new WI.EventListener(this, true);

        let managerResult = new Promise(function(resolve, reject) {
            listener.connect(WI.debuggerManager, WI.DebuggerManager.Event.ActiveCallFrameDidChange, resolve);
        });

        let protocolResult = this._activeCallFrame.target.DebuggerAgent.stepOut()
            .catch(function(error) {
                listener.disconnect();
                console.error("DebuggerManager.stepOut failed: ", error);
                throw error;
            });

        return Promise.all([managerResult, protocolResult]);
    }

    continueUntilNextRunLoop(target)
    {
        return this.dataForTarget(target).continueUntilNextRunLoop();
    }

    continueToLocation(script, lineNumber, columnNumber)
    {
        return script.target.DebuggerAgent.continueToLocation({scriptId: script.id, lineNumber, columnNumber});
    }

    addBreakpoint(breakpoint, shouldSpeculativelyResolve)
    {
        console.assert(breakpoint instanceof WI.Breakpoint);
        if (!breakpoint)
            return;

        if (this.isBreakpointSpecial(breakpoint)) {
            this.dispatchEventToListeners(WI.DebuggerManager.Event.BreakpointAdded, {breakpoint});
            return;
        }

        if (breakpoint.contentIdentifier) {
            let contentIdentifierBreakpoints = this._breakpointContentIdentifierMap.get(breakpoint.contentIdentifier);
            if (!contentIdentifierBreakpoints) {
                contentIdentifierBreakpoints = [];
                this._breakpointContentIdentifierMap.set(breakpoint.contentIdentifier, contentIdentifierBreakpoints);
            }
            contentIdentifierBreakpoints.push(breakpoint);
        }

        if (breakpoint.scriptIdentifier) {
            let scriptIdentifierBreakpoints = this._breakpointScriptIdentifierMap.get(breakpoint.scriptIdentifier);
            if (!scriptIdentifierBreakpoints) {
                scriptIdentifierBreakpoints = [];
                this._breakpointScriptIdentifierMap.set(breakpoint.scriptIdentifier, scriptIdentifierBreakpoints);
            }
            scriptIdentifierBreakpoints.push(breakpoint);
        }

        this._breakpoints.push(breakpoint);

        if (!breakpoint.disabled) {
            const specificTarget = undefined;
            this._setBreakpoint(breakpoint, specificTarget, () => {
                if (shouldSpeculativelyResolve)
                    breakpoint.resolved = true;
            });
        }

        this._saveBreakpoints();

        this._addProbesForBreakpoint(breakpoint);

        this.dispatchEventToListeners(WI.DebuggerManager.Event.BreakpointAdded, {breakpoint});
    }

    removeBreakpoint(breakpoint)
    {
        console.assert(breakpoint instanceof WI.Breakpoint);
        if (!breakpoint)
            return;

        console.assert(this.isBreakpointRemovable(breakpoint));
        if (!this.isBreakpointRemovable(breakpoint))
            return;

        if (this.isBreakpointSpecial(breakpoint)) {
            breakpoint.disabled = true;
            this.dispatchEventToListeners(WI.DebuggerManager.Event.BreakpointRemoved, {breakpoint});
            return;
        }

        this._breakpoints.remove(breakpoint);

        if (breakpoint.identifier)
            this._removeBreakpoint(breakpoint);

        if (breakpoint.contentIdentifier) {
            let contentIdentifierBreakpoints = this._breakpointContentIdentifierMap.get(breakpoint.contentIdentifier);
            if (contentIdentifierBreakpoints) {
                contentIdentifierBreakpoints.remove(breakpoint);
                if (!contentIdentifierBreakpoints.length)
                    this._breakpointContentIdentifierMap.delete(breakpoint.contentIdentifier);
            }
        }

        if (breakpoint.scriptIdentifier) {
            let scriptIdentifierBreakpoints = this._breakpointScriptIdentifierMap.get(breakpoint.scriptIdentifier);
            if (scriptIdentifierBreakpoints) {
                scriptIdentifierBreakpoints.remove(breakpoint);
                if (!scriptIdentifierBreakpoints.length)
                    this._breakpointScriptIdentifierMap.delete(breakpoint.scriptIdentifier);
            }
        }

        // Disable the breakpoint first, so removing actions doesn't re-add the breakpoint.
        breakpoint.disabled = true;
        breakpoint.clearActions();

        this._saveBreakpoints();

        this._removeProbesForBreakpoint(breakpoint);

        this.dispatchEventToListeners(WI.DebuggerManager.Event.BreakpointRemoved, {breakpoint});
    }

    nextBreakpointActionIdentifier()
    {
        return this._nextBreakpointActionIdentifier++;
    }

    initializeTarget(target)
    {
        let DebuggerAgent = target.DebuggerAgent;
        let targetData = this.dataForTarget(target);

        // Initialize global state.
        DebuggerAgent.enable();
        DebuggerAgent.setBreakpointsActive(this._breakpointsEnabledSetting.value);
        DebuggerAgent.setPauseOnAssertions(this._assertionFailuresBreakpointEnabledSetting.value);
        DebuggerAgent.setPauseOnExceptions(this._breakOnExceptionsState);
        DebuggerAgent.setAsyncStackTraceDepth(this._asyncStackTraceDepthSetting.value);

        if (this.paused)
            targetData.pauseIfNeeded();

        // Initialize breakpoints.
        this._restoringBreakpoints = true;
        for (let breakpoint of this._breakpoints) {
            if (breakpoint.disabled)
                continue;
            if (!breakpoint.contentIdentifier)
                continue;
            this._setBreakpoint(breakpoint, target);
        }
        this._restoringBreakpoints = false;
    }

    // Protected (Called from WI.DebuggerObserver)

    breakpointResolved(target, breakpointIdentifier, location)
    {
        // Called from WI.DebuggerObserver.

        let breakpoint = this._breakpointIdMap.get(breakpointIdentifier);
        console.assert(breakpoint);
        if (!breakpoint)
            return;

        console.assert(breakpoint.identifier === breakpointIdentifier);

        if (!breakpoint.sourceCodeLocation.sourceCode) {
            let sourceCodeLocation = this._sourceCodeLocationFromPayload(target, location);
            breakpoint.sourceCodeLocation.sourceCode = sourceCodeLocation.sourceCode;
        }

        breakpoint.resolved = true;
    }

    reset()
    {
        // Called from WI.DebuggerObserver.

        let wasPaused = this.paused;

        WI.Script.resetUniqueDisplayNameNumbers();

        this._internalWebKitScripts = [];
        this._targetDebuggerDataMap.clear();

        this._ignoreBreakpointDisplayLocationDidChangeEvent = true;

        // Mark all the breakpoints as unresolved. They will be reported as resolved when
        // breakpointResolved is called as the page loads.
        for (let breakpoint of this._breakpoints) {
            breakpoint.resolved = false;
            if (breakpoint.sourceCodeLocation.sourceCode)
                breakpoint.sourceCodeLocation.sourceCode = null;
        }

        this._ignoreBreakpointDisplayLocationDidChangeEvent = false;

        this.dispatchEventToListeners(WI.DebuggerManager.Event.ScriptsCleared);

        if (wasPaused)
            this.dispatchEventToListeners(WI.DebuggerManager.Event.Resumed);
    }

    debuggerDidPause(target, callFramesPayload, reason, data, asyncStackTracePayload)
    {
        // Called from WI.DebuggerObserver.

        if (this._delayedResumeTimeout) {
            clearTimeout(this._delayedResumeTimeout);
            this._delayedResumeTimeout = undefined;
        }

        let wasPaused = this.paused;
        let targetData = this._targetDebuggerDataMap.get(target);

        let callFrames = [];
        let pauseReason = this._pauseReasonFromPayload(reason);
        let pauseData = data || null;

        for (var i = 0; i < callFramesPayload.length; ++i) {
            var callFramePayload = callFramesPayload[i];
            var sourceCodeLocation = this._sourceCodeLocationFromPayload(target, callFramePayload.location);
            // FIXME: There may be useful call frames without a source code location (native callframes), should we include them?
            if (!sourceCodeLocation)
                continue;
            if (!sourceCodeLocation.sourceCode)
                continue;

            // Exclude the case where the call frame is in the inspector code.
            if (!WI.isDebugUIEnabled() && isWebKitInternalScript(sourceCodeLocation.sourceCode.sourceURL))
                continue;

            let scopeChain = this._scopeChainFromPayload(target, callFramePayload.scopeChain);
            let callFrame = WI.CallFrame.fromDebuggerPayload(target, callFramePayload, scopeChain, sourceCodeLocation);
            callFrames.push(callFrame);
        }

        let activeCallFrame = callFrames[0];

        if (!activeCallFrame) {
            // FIXME: This may not be safe for multiple threads/targets.
            // This indicates we were pausing in internal scripts only (Injected Scripts).
            // Just resume and skip past this pause. We should be fixing the backend to
            // not send such pauses.
            if (wasPaused)
                target.DebuggerAgent.continueUntilNextRunLoop();
            else
                target.DebuggerAgent.resume();
            this._didResumeInternal(target);
            return;
        }

        let asyncStackTrace = WI.StackTrace.fromPayload(target, asyncStackTracePayload);
        targetData.updateForPause(callFrames, pauseReason, pauseData, asyncStackTrace);

        // Pause other targets because at least one target has paused.
        // FIXME: Should this be done on the backend?
        for (let [otherTarget, otherTargetData] of this._targetDebuggerDataMap)
            otherTargetData.pauseIfNeeded();

        let activeCallFrameDidChange = this._activeCallFrame && this._activeCallFrame.target === target;
        if (activeCallFrameDidChange)
            this._activeCallFrame = activeCallFrame;
        else if (!wasPaused) {
            this._activeCallFrame = activeCallFrame;
            activeCallFrameDidChange = true;
        }

        if (!wasPaused)
            this.dispatchEventToListeners(WI.DebuggerManager.Event.Paused);

        this.dispatchEventToListeners(WI.DebuggerManager.Event.CallFramesDidChange, {target});

        if (activeCallFrameDidChange)
            this.dispatchEventToListeners(WI.DebuggerManager.Event.ActiveCallFrameDidChange);
    }

    debuggerDidResume(target)
    {
        // Called from WI.DebuggerObserver.

        // COMPATIBILITY (iOS 10): Debugger.resumed event was ambiguous. When stepping
        // we would receive a Debugger.resumed and we would not know if it really meant
        // the backend resumed or would pause again due to a step. Legacy backends wait
        // 50ms, and treat it as a real resume if we haven't paused in that time frame.
        // This delay ensures the user interface does not flash between brief steps
        // or successive breakpoints.
        if (!DebuggerAgent.setPauseOnAssertions) {
            this._delayedResumeTimeout = setTimeout(this._didResumeInternal.bind(this, target), 50);
            return;
        }

        this._didResumeInternal(target);
    }

    playBreakpointActionSound(breakpointActionIdentifier)
    {
        // Called from WI.DebuggerObserver.

        InspectorFrontendHost.beep();
    }

    scriptDidParse(target, scriptIdentifier, url, startLine, startColumn, endLine, endColumn, isModule, isContentScript, sourceURL, sourceMapURL)
    {
        // Called from WI.DebuggerObserver.

        // Don't add the script again if it is already known.
        let targetData = this.dataForTarget(target);
        let existingScript = targetData.scriptForIdentifier(scriptIdentifier);
        if (existingScript) {
            console.assert(existingScript.url === (url || null));
            console.assert(existingScript.range.startLine === startLine);
            console.assert(existingScript.range.startColumn === startColumn);
            console.assert(existingScript.range.endLine === endLine);
            console.assert(existingScript.range.endColumn === endColumn);
            return;
        }

        if (!WI.isDebugUIEnabled() && isWebKitInternalScript(sourceURL))
            return;

        let range = new WI.TextRange(startLine, startColumn, endLine, endColumn);
        let sourceType = isModule ? WI.Script.SourceType.Module : WI.Script.SourceType.Program;
        let script = new WI.Script(target, scriptIdentifier, range, url, sourceType, isContentScript, sourceURL, sourceMapURL);

        targetData.addScript(script);

        // FIXME: <https://webkit.org/b/164427> Web Inspector: WorkerTarget's mainResource should be a Resource not a Script
        // We make the main resource of a WorkerTarget the Script instead of the Resource
        // because the frontend may not be informed of the Resource. We should guarantee
        // the frontend is informed of the Resource.
        if (WI.sharedApp.debuggableType === WI.DebuggableType.ServiceWorker) {
            // A ServiceWorker starts with a LocalScript for the main resource but we can replace it during initialization.
            if (target.mainResource instanceof WI.LocalScript) {
                if (script.url === target.name)
                    target.mainResource = script;
            }
        } else if (!target.mainResource && target !== WI.mainTarget) {
            // A Worker starts without a main resource and we insert one.
            if (script.url === target.name) {
                target.mainResource = script;
                if (script.resource)
                    target.resourceCollection.remove(script.resource);
            }
        }

        if (isWebKitInternalScript(script.sourceURL)) {
            this._internalWebKitScripts.push(script);
            if (!WI.isDebugUIEnabled())
                return;
        }

        // Console expressions are not added to the UI by default.
        if (isWebInspectorConsoleEvaluationScript(script.sourceURL))
            return;

        this.dispatchEventToListeners(WI.DebuggerManager.Event.ScriptAdded, {script});

        if ((target !== WI.mainTarget || WI.sharedApp.debuggableType === WI.DebuggableType.ServiceWorker) && !script.isMainResource() && !script.resource)
            target.addScript(script);
    }

    didSampleProbe(target, sample)
    {
        console.assert(this._probesByIdentifier.has(sample.probeId), "Unknown probe identifier specified for sample: ", sample);
        let probe = this._probesByIdentifier.get(sample.probeId);
        let elapsedTime = WI.timelineManager.computeElapsedTime(sample.timestamp);
        let object = WI.RemoteObject.fromPayload(sample.payload, target);
        probe.addSample(new WI.ProbeSample(sample.sampleId, sample.batchId, elapsedTime, object));
    }

    // Private

    _sourceCodeLocationFromPayload(target, payload)
    {
        let targetData = this.dataForTarget(target);
        let script = targetData.scriptForIdentifier(payload.scriptId);
        if (!script)
            return null;

        return script.createSourceCodeLocation(payload.lineNumber, payload.columnNumber);
    }

    _scopeChainFromPayload(target, payload)
    {
        let scopeChain = [];
        for (let i = 0; i < payload.length; ++i)
            scopeChain.push(this._scopeChainNodeFromPayload(target, payload[i]));
        return scopeChain;
    }

    _scopeChainNodeFromPayload(target, payload)
    {
        var type = null;
        switch (payload.type) {
        case DebuggerAgent.ScopeType.Global:
            type = WI.ScopeChainNode.Type.Global;
            break;
        case DebuggerAgent.ScopeType.With:
            type = WI.ScopeChainNode.Type.With;
            break;
        case DebuggerAgent.ScopeType.Closure:
            type = WI.ScopeChainNode.Type.Closure;
            break;
        case DebuggerAgent.ScopeType.Catch:
            type = WI.ScopeChainNode.Type.Catch;
            break;
        case DebuggerAgent.ScopeType.FunctionName:
            type = WI.ScopeChainNode.Type.FunctionName;
            break;
        case DebuggerAgent.ScopeType.NestedLexical:
            type = WI.ScopeChainNode.Type.Block;
            break;
        case DebuggerAgent.ScopeType.GlobalLexicalEnvironment:
            type = WI.ScopeChainNode.Type.GlobalLexicalEnvironment;
            break;

        // COMPATIBILITY (iOS 9): Debugger.ScopeType.Local used to be provided by the backend.
        // Newer backends no longer send this enum value, it should be computed by the frontend.
        // Map this to "Closure" type. The frontend can recalculate this when needed.
        case DebuggerAgent.ScopeType.Local:
            type = WI.ScopeChainNode.Type.Closure;
            break;

        default:
            console.error("Unknown type: " + payload.type);
        }

        let object = WI.RemoteObject.fromPayload(payload.object, target);
        return new WI.ScopeChainNode(type, [object], payload.name, payload.location, payload.empty);
    }

    _pauseReasonFromPayload(payload)
    {
        // FIXME: Handle other backend pause reasons.
        switch (payload) {
        case DebuggerAgent.PausedReason.AnimationFrame:
            return WI.DebuggerManager.PauseReason.AnimationFrame;
        case DebuggerAgent.PausedReason.Assert:
            return WI.DebuggerManager.PauseReason.Assertion;
        case DebuggerAgent.PausedReason.Breakpoint:
            return WI.DebuggerManager.PauseReason.Breakpoint;
        case DebuggerAgent.PausedReason.CSPViolation:
            return WI.DebuggerManager.PauseReason.CSPViolation;
        case DebuggerAgent.PausedReason.DOM:
            return WI.DebuggerManager.PauseReason.DOM;
        case DebuggerAgent.PausedReason.DebuggerStatement:
            return WI.DebuggerManager.PauseReason.DebuggerStatement;
        case DebuggerAgent.PausedReason.EventListener:
            return WI.DebuggerManager.PauseReason.EventListener;
        case DebuggerAgent.PausedReason.Exception:
            return WI.DebuggerManager.PauseReason.Exception;
        case DebuggerAgent.PausedReason.PauseOnNextStatement:
            return WI.DebuggerManager.PauseReason.PauseOnNextStatement;
        case DebuggerAgent.PausedReason.Timer:
            return WI.DebuggerManager.PauseReason.Timer;
        case DebuggerAgent.PausedReason.XHR:
            return WI.DebuggerManager.PauseReason.XHR;
        default:
            return WI.DebuggerManager.PauseReason.Other;
        }
    }

    _debuggerBreakpointActionType(type)
    {
        switch (type) {
        case WI.BreakpointAction.Type.Log:
            return DebuggerAgent.BreakpointActionType.Log;
        case WI.BreakpointAction.Type.Evaluate:
            return DebuggerAgent.BreakpointActionType.Evaluate;
        case WI.BreakpointAction.Type.Sound:
            return DebuggerAgent.BreakpointActionType.Sound;
        case WI.BreakpointAction.Type.Probe:
            return DebuggerAgent.BreakpointActionType.Probe;
        default:
            console.assert(false);
            return DebuggerAgent.BreakpointActionType.Log;
        }
    }

    _debuggerBreakpointOptions(breakpoint)
    {
        const templatePlaceholderRegex = /\$\{.*?\}/;

        let options = breakpoint.options;
        let invalidActions = [];

        for (let action of options.actions) {
            if (action.type !== WI.BreakpointAction.Type.Log)
                continue;

            if (!templatePlaceholderRegex.test(action.data))
                continue;

            let lexer = new WI.BreakpointLogMessageLexer;
            let tokens = lexer.tokenize(action.data);
            if (!tokens) {
                invalidActions.push(action);
                continue;
            }

            let templateLiteral = tokens.reduce((text, token) => {
                if (token.type === WI.BreakpointLogMessageLexer.TokenType.PlainText)
                    return text + token.data.escapeCharacters("`\\");
                if (token.type === WI.BreakpointLogMessageLexer.TokenType.Expression)
                    return text + "${" + token.data + "}";
                return text;
            }, "");

            action.data = "console.log(`" + templateLiteral + "`)";
            action.type = WI.BreakpointAction.Type.Evaluate;
        }

        for (let invalidAction of invalidActions)
            options.actions.remove(invalidAction);

        return options;
    }

    _setBreakpoint(breakpoint, specificTarget, callback)
    {
        console.assert(!breakpoint.disabled);

        if (breakpoint.disabled)
            return;

        if (!this._restoringBreakpoints && !this.breakpointsDisabledTemporarily) {
            // Enable breakpoints since a breakpoint is being set. This eliminates
            // a multi-step process for the user that can be confusing.
            this.breakpointsEnabled = true;
        }

        function didSetBreakpoint(target, error, breakpointIdentifier, locations)
        {
            if (error)
                return;

            this._breakpointIdMap.set(breakpointIdentifier, breakpoint);

            breakpoint.identifier = breakpointIdentifier;

            // Debugger.setBreakpoint returns a single location.
            if (!(locations instanceof Array))
                locations = [locations];

            for (let location of locations)
                this.breakpointResolved(target, breakpointIdentifier, location);

            if (typeof callback === "function")
                callback();
        }

        // The breakpoint will be resolved again by calling DebuggerAgent, so mark it as unresolved.
        // If something goes wrong it will stay unresolved and show up as such in the user interface.
        // When setting for a new target, don't change the resolved target.
        if (!specificTarget)
            breakpoint.resolved = false;

        // Convert BreakpointAction types to DebuggerAgent protocol types.
        // NOTE: Breakpoint.options returns new objects each time, so it is safe to modify.
        let options = this._debuggerBreakpointOptions(breakpoint);
        if (options.actions.length) {
            for (let action of options.actions)
                action.type = this._debuggerBreakpointActionType(action.type);
        }

        if (breakpoint.contentIdentifier) {
            let targets = specificTarget ? [specificTarget] : WI.targets;
            for (let target of targets) {
                target.DebuggerAgent.setBreakpointByUrl.invoke({
                    lineNumber: breakpoint.sourceCodeLocation.lineNumber,
                    url: breakpoint.contentIdentifier,
                    urlRegex: undefined,
                    columnNumber: breakpoint.sourceCodeLocation.columnNumber,
                    options
                }, didSetBreakpoint.bind(this, target), target.DebuggerAgent);
            }
        } else if (breakpoint.scriptIdentifier) {
            let target = breakpoint.target;
            target.DebuggerAgent.setBreakpoint.invoke({
                location: {scriptId: breakpoint.scriptIdentifier, lineNumber: breakpoint.sourceCodeLocation.lineNumber, columnNumber: breakpoint.sourceCodeLocation.columnNumber},
                options
            }, didSetBreakpoint.bind(this, target), target.DebuggerAgent);
        }
    }

    _removeBreakpoint(breakpoint, callback)
    {
        if (!breakpoint.identifier)
            return;

        function didRemoveBreakpoint(error)
        {
            if (error)
                console.error(error);

            this._breakpointIdMap.delete(breakpoint.identifier);

            breakpoint.identifier = null;

            // Don't reset resolved here since we want to keep disabled breakpoints looking like they
            // are resolved in the user interface. They will get marked as unresolved in reset.

            if (typeof callback === "function")
                callback();
        }

        if (breakpoint.contentIdentifier) {
            for (let target of WI.targets)
                target.DebuggerAgent.removeBreakpoint(breakpoint.identifier, didRemoveBreakpoint.bind(this));
        } else if (breakpoint.scriptIdentifier) {
            let target = breakpoint.target;
            target.DebuggerAgent.removeBreakpoint(breakpoint.identifier, didRemoveBreakpoint.bind(this));
        }
    }

    _breakpointDisplayLocationDidChange(event)
    {
        if (this._ignoreBreakpointDisplayLocationDidChangeEvent)
            return;

        let breakpoint = event.target;
        if (!breakpoint.identifier || breakpoint.disabled)
            return;

        // Remove the breakpoint with its old id.
        this._removeBreakpoint(breakpoint, breakpointRemoved.bind(this));

        function breakpointRemoved()
        {
            // Add the breakpoint at its new lineNumber and get a new id.
            this._setBreakpoint(breakpoint);

            this.dispatchEventToListeners(WI.DebuggerManager.Event.BreakpointMoved, {breakpoint});
        }
    }

    _breakpointDisabledStateDidChange(event)
    {
        this._saveBreakpoints();

        let breakpoint = event.target;
        if (breakpoint === this._allExceptionsBreakpoint) {
            if (!breakpoint.disabled && !this.breakpointsDisabledTemporarily)
                this.breakpointsEnabled = true;
            this._allExceptionsBreakpointEnabledSetting.value = !breakpoint.disabled;
            this._updateBreakOnExceptionsState();
            for (let target of WI.targets)
                target.DebuggerAgent.setPauseOnExceptions(this._breakOnExceptionsState);
            return;
        }

        if (breakpoint === this._uncaughtExceptionsBreakpoint) {
            if (!breakpoint.disabled && !this.breakpointsDisabledTemporarily)
                this.breakpointsEnabled = true;
            this._uncaughtExceptionsBreakpointEnabledSetting.value = !breakpoint.disabled;
            this._updateBreakOnExceptionsState();
            for (let target of WI.targets)
                target.DebuggerAgent.setPauseOnExceptions(this._breakOnExceptionsState);
            return;
        }

        if (breakpoint === this._assertionFailuresBreakpoint) {
            if (!breakpoint.disabled && !this.breakpointsDisabledTemporarily)
                this.breakpointsEnabled = true;
            this._assertionFailuresBreakpointEnabledSetting.value = !breakpoint.disabled;
            for (let target of WI.targets)
                target.DebuggerAgent.setPauseOnAssertions(this._assertionFailuresBreakpointEnabledSetting.value);
            return;
        }

        if (breakpoint.disabled)
            this._removeBreakpoint(breakpoint);
        else
            this._setBreakpoint(breakpoint);
    }

    _breakpointEditablePropertyDidChange(event)
    {
        this._saveBreakpoints();

        let breakpoint = event.target;
        if (breakpoint.disabled)
            return;

        console.assert(this.isBreakpointEditable(breakpoint));
        if (!this.isBreakpointEditable(breakpoint))
            return;

        // Remove the breakpoint with its old id.
        this._removeBreakpoint(breakpoint, breakpointRemoved.bind(this));

        function breakpointRemoved()
        {
            // Add the breakpoint with its new properties and get a new id.
            this._setBreakpoint(breakpoint);
        }
    }

    _handleBreakpointActionsDidChange(event)
    {
        this._breakpointEditablePropertyDidChange(event);

        this._updateProbesForBreakpoint(event.target);
    }

    _startDisablingBreakpointsTemporarily()
    {
        console.assert(!this.breakpointsDisabledTemporarily, "Already temporarily disabling breakpoints.");
        if (this.breakpointsDisabledTemporarily)
            return;

        this._temporarilyDisabledBreakpointsRestoreSetting.value = this._breakpointsEnabledSetting.value;

        this.breakpointsEnabled = false;
    }

    _stopDisablingBreakpointsTemporarily()
    {
        console.assert(this.breakpointsDisabledTemporarily, "Was not temporarily disabling breakpoints.");
        if (!this.breakpointsDisabledTemporarily)
            return;

        let restoreState = this._temporarilyDisabledBreakpointsRestoreSetting.value;
        this._temporarilyDisabledBreakpointsRestoreSetting.value = null;

        this.breakpointsEnabled = restoreState;
    }

    _timelineCapturingWillStart(event)
    {
        this._startDisablingBreakpointsTemporarily();

        if (this.paused)
            this.resume();
    }

    _timelineCapturingStopped(event)
    {
        this._stopDisablingBreakpointsTemporarily();
    }

    _targetRemoved(event)
    {
        let wasPaused = this.paused;

        this._targetDebuggerDataMap.delete(event.data.target);

        if (!this.paused && wasPaused)
            this.dispatchEventToListeners(WI.DebuggerManager.Event.Resumed);
    }

    _mainResourceDidChange(event)
    {
        if (!event.target.isMainFrame())
            return;

        this._didResumeInternal(WI.mainTarget);
    }

    _didResumeInternal(target)
    {
        if (!this.paused)
            return;

        if (this._delayedResumeTimeout) {
            clearTimeout(this._delayedResumeTimeout);
            this._delayedResumeTimeout = undefined;
        }

        let activeCallFrameDidChange = false;
        if (this._activeCallFrame && this._activeCallFrame.target === target) {
            this._activeCallFrame = null;
            activeCallFrameDidChange = true;
        }

        this.dataForTarget(target).updateForResume();

        if (!this.paused)
            this.dispatchEventToListeners(WI.DebuggerManager.Event.Resumed);

        this.dispatchEventToListeners(WI.DebuggerManager.Event.CallFramesDidChange, {target});

        if (activeCallFrameDidChange)
            this.dispatchEventToListeners(WI.DebuggerManager.Event.ActiveCallFrameDidChange);
    }

    _updateBreakOnExceptionsState()
    {
        let state = "none";

        if (this._breakpointsEnabledSetting.value) {
            if (!this._allExceptionsBreakpoint.disabled)
                state = "all";
            else if (!this._uncaughtExceptionsBreakpoint.disabled)
                state = "uncaught";
        }

        this._breakOnExceptionsState = state;

        switch (state) {
        case "all":
            // Mark the uncaught breakpoint as unresolved since "all" includes "uncaught".
            // That way it is clear in the user interface that the breakpoint is ignored.
            this._uncaughtExceptionsBreakpoint.resolved = false;
            break;
        case "uncaught":
        case "none":
            // Mark the uncaught breakpoint as resolved again.
            this._uncaughtExceptionsBreakpoint.resolved = true;
            break;
        }
    }

    _saveBreakpoints()
    {
        if (this._restoringBreakpoints)
            return;

        let breakpointsToSave = this._breakpoints.filter((breakpoint) => !!breakpoint.contentIdentifier);
        let serializedBreakpoints = breakpointsToSave.map((breakpoint) => breakpoint.info);
        this._breakpointsSetting.value = serializedBreakpoints;
    }

    _associateBreakpointsWithSourceCode(breakpoints, sourceCode)
    {
        this._ignoreBreakpointDisplayLocationDidChangeEvent = true;

        for (let breakpoint of breakpoints) {
            if (!breakpoint.sourceCodeLocation.sourceCode)
                breakpoint.sourceCodeLocation.sourceCode = sourceCode;
            // SourceCodes can be unequal if the SourceCodeLocation is associated with a Script and we are looking at the Resource.
            console.assert(breakpoint.sourceCodeLocation.sourceCode === sourceCode || breakpoint.sourceCodeLocation.sourceCode.contentIdentifier === sourceCode.contentIdentifier);
        }

        this._ignoreBreakpointDisplayLocationDidChangeEvent = false;
    }

    _addProbesForBreakpoint(breakpoint)
    {
        if (this._knownProbeIdentifiersForBreakpoint.has(breakpoint))
            return;

        this._knownProbeIdentifiersForBreakpoint.set(breakpoint, new Set);

        this._updateProbesForBreakpoint(breakpoint);
    }

    _removeProbesForBreakpoint(breakpoint)
    {
        console.assert(this._knownProbeIdentifiersForBreakpoint.has(breakpoint));

        this._updateProbesForBreakpoint(breakpoint);
        this._knownProbeIdentifiersForBreakpoint.delete(breakpoint);
    }

    _updateProbesForBreakpoint(breakpoint)
    {
        let knownProbeIdentifiers = this._knownProbeIdentifiersForBreakpoint.get(breakpoint);
        if (!knownProbeIdentifiers) {
            // Sometimes actions change before the added breakpoint is fully dispatched.
            this._addProbesForBreakpoint(breakpoint);
            return;
        }

        let seenProbeIdentifiers = new Set;

        for (let probeAction of breakpoint.probeActions) {
            let probeIdentifier = probeAction.id;
            console.assert(probeIdentifier, "Probe added without breakpoint action identifier: ", breakpoint);

            seenProbeIdentifiers.add(probeIdentifier);
            if (!knownProbeIdentifiers.has(probeIdentifier)) {
                // New probe; find or create relevant probe set.
                knownProbeIdentifiers.add(probeIdentifier);
                let probeSet = this._probeSetForBreakpoint(breakpoint);
                let newProbe = new WI.Probe(probeIdentifier, breakpoint, probeAction.data);
                this._probesByIdentifier.set(probeIdentifier, newProbe);
                probeSet.addProbe(newProbe);
                break;
            }

            let probe = this._probesByIdentifier.get(probeIdentifier);
            console.assert(probe, "Probe known but couldn't be found by identifier: ", probeIdentifier);
            // Update probe expression; if it differed, change events will fire.
            probe.expression = probeAction.data;
        }

        // Look for missing probes based on what we saw last.
        for (let probeIdentifier of knownProbeIdentifiers) {
            if (seenProbeIdentifiers.has(probeIdentifier))
                break;

            // The probe has gone missing, remove it.
            let probeSet = this._probeSetForBreakpoint(breakpoint);
            let probe = this._probesByIdentifier.get(probeIdentifier);
            this._probesByIdentifier.delete(probeIdentifier);
            knownProbeIdentifiers.delete(probeIdentifier);
            probeSet.removeProbe(probe);

            // Remove the probe set if it has become empty.
            if (!probeSet.probes.length) {
                this._probeSetsByBreakpoint.delete(probeSet.breakpoint);
                probeSet.willRemove();
                this.dispatchEventToListeners(WI.DebuggerManager.Event.ProbeSetRemoved, {probeSet});
            }
        }
    }

    _probeSetForBreakpoint(breakpoint)
    {
        let probeSet = this._probeSetsByBreakpoint.get(breakpoint);
        if (!probeSet) {
            probeSet = new WI.ProbeSet(breakpoint);
            this._probeSetsByBreakpoint.set(breakpoint, probeSet);
            this.dispatchEventToListeners(WI.DebuggerManager.Event.ProbeSetAdded, {probeSet});
        }
        return probeSet;
    }

    _debugUIEnabledDidChange()
    {
        let eventType = WI.isDebugUIEnabled() ? WI.DebuggerManager.Event.ScriptAdded : WI.DebuggerManager.Event.ScriptRemoved;
        for (let script of this._internalWebKitScripts)
            this.dispatchEventToListeners(eventType, {script});
    }
};

WI.DebuggerManager.Event = {
    BreakpointAdded: "debugger-manager-breakpoint-added",
    BreakpointRemoved: "debugger-manager-breakpoint-removed",
    BreakpointMoved: "debugger-manager-breakpoint-moved",
    WaitingToPause: "debugger-manager-waiting-to-pause",
    Paused: "debugger-manager-paused",
    Resumed: "debugger-manager-resumed",
    CallFramesDidChange: "debugger-manager-call-frames-did-change",
    ActiveCallFrameDidChange: "debugger-manager-active-call-frame-did-change",
    ScriptAdded: "debugger-manager-script-added",
    ScriptRemoved: "debugger-manager-script-removed",
    ScriptsCleared: "debugger-manager-scripts-cleared",
    BreakpointsEnabledDidChange: "debugger-manager-breakpoints-enabled-did-change",
    ProbeSetAdded: "debugger-manager-probe-set-added",
    ProbeSetRemoved: "debugger-manager-probe-set-removed",
};

WI.DebuggerManager.PauseReason = {
    AnimationFrame: "animation-frame",
    Assertion: "assertion",
    Breakpoint: "breakpoint",
    CSPViolation: "CSP-violation",
    DebuggerStatement: "debugger-statement",
    DOM: "DOM",
    EventListener: "event-listener",
    Exception: "exception",
    PauseOnNextStatement: "pause-on-next-statement",
    Timer: "timer",
    XHR: "xhr",
    Other: "other",
};
