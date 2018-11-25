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

WI.ScriptTimelineRecord = class ScriptTimelineRecord extends WI.TimelineRecord
{
    constructor(eventType, startTime, endTime, callFrames, sourceCodeLocation, details, profilePayload)
    {
        super(WI.TimelineRecord.Type.Script, startTime, endTime, callFrames, sourceCodeLocation);

        console.assert(eventType);

        if (eventType in WI.ScriptTimelineRecord.EventType)
            eventType = WI.ScriptTimelineRecord.EventType[eventType];

        this._eventType = eventType;
        this._details = details || "";
        this._profilePayload = profilePayload || null;
        this._profile = null;

        // COMPATIBILITY(iOS 9): Before the ScriptProfilerAgent we did not have sample data. Return NaN to match old behavior.
        if (!window.ScriptProfilerAgent)
            this._callCountOrSamples = NaN;
        else {
            // NOTE: _callCountOrSamples is being treated as the number of samples.
            this._callCountOrSamples = 0;
        }
    }

    // Public

    get eventType()
    {
        return this._eventType;
    }

    get details()
    {
        return this._details;
    }

    get profile()
    {
        this._initializeProfileFromPayload();
        return this._profile;
    }

    get callCountOrSamples()
    {
        return this._callCountOrSamples;
    }

    isGarbageCollection()
    {
        return this._eventType === WI.ScriptTimelineRecord.EventType.GarbageCollected;
    }

    saveIdentityToCookie(cookie)
    {
        super.saveIdentityToCookie(cookie);

        cookie[WI.ScriptTimelineRecord.EventTypeCookieKey] = this._eventType;
        cookie[WI.ScriptTimelineRecord.DetailsCookieKey] = this._details;
    }

    get profilePayload()
    {
        return this._profilePayload;
    }

    set profilePayload(payload)
    {
        this._profilePayload = payload;
    }

    // Private

    _initializeProfileFromPayload(payload)
    {
        if (this._profile || !this._profilePayload)
            return;

        var payload = this._profilePayload;
        this._profilePayload = undefined;

        console.assert(payload.rootNodes instanceof Array);

        function profileNodeFromPayload(nodePayload)
        {
            console.assert("id" in nodePayload);

            if (nodePayload.url) {
                var sourceCode = WI.networkManager.resourceForURL(nodePayload.url);
                if (!sourceCode)
                    sourceCode = WI.debuggerManager.scriptsForURL(nodePayload.url, WI.assumingMainTarget())[0];

                // The lineNumber is 1-based, but we expect 0-based.
                var lineNumber = nodePayload.lineNumber - 1;

                var sourceCodeLocation = sourceCode ? sourceCode.createLazySourceCodeLocation(lineNumber, nodePayload.columnNumber) : null;
            }

            var isProgramCode = nodePayload.functionName === "(program)";
            var isAnonymousFunction = nodePayload.functionName === "(anonymous function)";

            var type = isProgramCode ? WI.ProfileNode.Type.Program : WI.ProfileNode.Type.Function;
            var functionName = !isProgramCode && !isAnonymousFunction && nodePayload.functionName !== "(unknown)" ? nodePayload.functionName : null;

            // COMPATIBILITY (iOS 8): Timeline.CPUProfileNodes used to include an array of complete
            // call information instead of the aggregated "callInfo" data.
            var calls = null;
            if ("calls" in nodePayload) {
                console.assert(nodePayload.calls instanceof Array);
                calls = nodePayload.calls.map(profileNodeCallFromPayload);
            }

            return new WI.ProfileNode(nodePayload.id, type, functionName, sourceCodeLocation, nodePayload.callInfo, calls, nodePayload.children);
        }

        function profileNodeCallFromPayload(nodeCallPayload)
        {
            console.assert("startTime" in nodeCallPayload);
            console.assert("totalTime" in nodeCallPayload);

            var startTime = WI.timelineManager.computeElapsedTime(nodeCallPayload.startTime);

            return new WI.ProfileNodeCall(startTime, nodeCallPayload.totalTime);
        }

        var rootNodes = payload.rootNodes;

        // Iterate over the node tree using a stack. Doing this recursively can easily cause a stack overflow.
        // We traverse the profile in post-order and convert the payloads in place until we get back to the root.
        var stack = [{parent: {children: rootNodes}, index: 0, root: true}];
        while (stack.length) {
            var entry = stack.lastValue;

            if (entry.index < entry.parent.children.length) {
                var childNodePayload = entry.parent.children[entry.index];
                if (childNodePayload.children && childNodePayload.children.length)
                    stack.push({parent: childNodePayload, index: 0});

                ++entry.index;
            } else {
                if (!entry.root)
                    entry.parent.children = entry.parent.children.map(profileNodeFromPayload);
                else
                    rootNodes = rootNodes.map(profileNodeFromPayload);

                stack.pop();
            }
        }

        // COMPATIBILITY (iOS 9): We only do this when we have ScriptProfilerAgent because before that we didn't have a Sampling Profiler.
        if (window.ScriptProfilerAgent) {
            for (let i = 0; i < rootNodes.length; i++)
                this._callCountOrSamples += rootNodes[i].callInfo.callCount;
        }

        this._profile = new WI.Profile(rootNodes);
    }
};

WI.ScriptTimelineRecord.EventType = {
    ScriptEvaluated: "script-timeline-record-script-evaluated",
    APIScriptEvaluated: "script-timeline-record-api-script-evaluated",
    MicrotaskDispatched: "script-timeline-record-microtask-dispatched",
    EventDispatched: "script-timeline-record-event-dispatched",
    ProbeSampleRecorded: "script-timeline-record-probe-sample-recorded",
    TimerFired: "script-timeline-record-timer-fired",
    TimerInstalled: "script-timeline-record-timer-installed",
    TimerRemoved: "script-timeline-record-timer-removed",
    AnimationFrameFired: "script-timeline-record-animation-frame-fired",
    AnimationFrameRequested: "script-timeline-record-animation-frame-requested",
    AnimationFrameCanceled: "script-timeline-record-animation-frame-canceled",
    ConsoleProfileRecorded: "script-timeline-record-console-profile-recorded",
    GarbageCollected: "script-timeline-record-garbage-collected",
};

WI.ScriptTimelineRecord.EventType.displayName = function(eventType, details, includeDetailsInMainTitle)
{
    if (details && !WI.ScriptTimelineRecord._eventDisplayNames) {
        // These display names are not localized because they closely represent
        // the real API name, just with word spaces and Title Case.

        var nameMap = new Map;
        nameMap.set("DOMActivate", "DOM Activate");
        nameMap.set("DOMCharacterDataModified", "DOM Character Data Modified");
        nameMap.set("DOMContentLoaded", "DOM Content Loaded");
        nameMap.set("DOMFocusIn", "DOM Focus In");
        nameMap.set("DOMFocusOut", "DOM Focus Out");
        nameMap.set("DOMNodeInserted", "DOM Node Inserted");
        nameMap.set("DOMNodeInsertedIntoDocument", "DOM Node Inserted Into Document");
        nameMap.set("DOMNodeRemoved", "DOM Node Removed");
        nameMap.set("DOMNodeRemovedFromDocument", "DOM Node Removed From Document");
        nameMap.set("DOMSubtreeModified", "DOM Sub-Tree Modified");
        nameMap.set("addsourcebuffer", "Add Source Buffer");
        nameMap.set("addstream", "Add Stream");
        nameMap.set("addtrack", "Add Track");
        nameMap.set("animationend", "Animation End");
        nameMap.set("animationiteration", "Animation Iteration");
        nameMap.set("animationstart", "Animation Start");
        nameMap.set("audioend", "Audio End");
        nameMap.set("audioprocess", "Audio Process");
        nameMap.set("audiostart", "Audio Start");
        nameMap.set("beforecopy", "Before Copy");
        nameMap.set("beforecut", "Before Cut");
        nameMap.set("beforeload", "Before Load");
        nameMap.set("beforepaste", "Before Paste");
        nameMap.set("beforeunload", "Before Unload");
        nameMap.set("canplay", "Can Play");
        nameMap.set("canplaythrough", "Can Play Through");
        nameMap.set("chargingchange", "Charging Change");
        nameMap.set("chargingtimechange", "Charging Time Change");
        nameMap.set("compositionend", "Composition End");
        nameMap.set("compositionstart", "Composition Start");
        nameMap.set("compositionupdate", "Composition Update");
        nameMap.set("contextmenu", "Context Menu");
        nameMap.set("cuechange", "Cue Change");
        nameMap.set("datachannel", "Data Channel");
        nameMap.set("dblclick", "Double Click");
        nameMap.set("devicemotion", "Device Motion");
        nameMap.set("deviceorientation", "Device Orientation");
        nameMap.set("dischargingtimechange", "Discharging Time Change");
        nameMap.set("dragend", "Drag End");
        nameMap.set("dragenter", "Drag Enter");
        nameMap.set("dragleave", "Drag Leave");
        nameMap.set("dragover", "Drag Over");
        nameMap.set("dragstart", "Drag Start");
        nameMap.set("durationchange", "Duration Change");
        nameMap.set("focusin", "Focus In");
        nameMap.set("focusout", "Focus Out");
        nameMap.set("gesturechange", "Gesture Change");
        nameMap.set("gestureend", "Gesture End");
        nameMap.set("gesturescrollend", "Gesture Scroll End");
        nameMap.set("gesturescrollstart", "Gesture Scroll Start");
        nameMap.set("gesturescrollupdate", "Gesture Scroll Update");
        nameMap.set("gesturestart", "Gesture Start");
        nameMap.set("gesturetap", "Gesture Tap");
        nameMap.set("gesturetapdown", "Gesture Tap Down");
        nameMap.set("hashchange", "Hash Change");
        nameMap.set("icecandidate", "ICE Candidate");
        nameMap.set("iceconnectionstatechange", "ICE Connection State Change");
        nameMap.set("keydown", "Key Down");
        nameMap.set("keypress", "Key Press");
        nameMap.set("keyup", "Key Up");
        nameMap.set("levelchange", "Level Change");
        nameMap.set("loadeddata", "Loaded Data");
        nameMap.set("loadedmetadata", "Loaded Metadata");
        nameMap.set("loadend", "Load End");
        nameMap.set("loadingdone", "Loading Done");
        nameMap.set("loadstart", "Load Start");
        nameMap.set("mousedown", "Mouse Down");
        nameMap.set("mouseenter", "Mouse Enter");
        nameMap.set("mouseleave", "Mouse Leave");
        nameMap.set("mousemove", "Mouse Move");
        nameMap.set("mouseout", "Mouse Out");
        nameMap.set("mouseover", "Mouse Over");
        nameMap.set("mouseup", "Mouse Up");
        nameMap.set("mousewheel", "Mouse Wheel");
        nameMap.set("negotiationneeded", "Negotiation Needed");
        nameMap.set("nomatch", "No Match");
        nameMap.set("noupdate", "No Update");
        nameMap.set("orientationchange", "Orientation Change");
        nameMap.set("overflowchanged", "Overflow Changed");
        nameMap.set("pagehide", "Page Hide");
        nameMap.set("pageshow", "Page Show");
        nameMap.set("popstate", "Pop State");
        nameMap.set("ratechange", "Rate Change");
        nameMap.set("readystatechange", "Ready State Change");
        nameMap.set("removesourcebuffer", "Remove Source Buffer");
        nameMap.set("removestream", "Remove Stream");
        nameMap.set("removetrack", "Remove Track");
        nameMap.set("resize", "Resize");
        nameMap.set("securitypolicyviolation", "Security Policy Violation");
        nameMap.set("selectionchange", "Selection Change");
        nameMap.set("selectstart", "Select Start");
        nameMap.set("signalingstatechange", "Signaling State Change");
        nameMap.set("soundend", "Sound End");
        nameMap.set("soundstart", "Sound Start");
        nameMap.set("sourceclose", "Source Close");
        nameMap.set("sourceended", "Source Ended");
        nameMap.set("sourceopen", "Source Open");
        nameMap.set("speechend", "Speech End");
        nameMap.set("speechstart", "Speech Start");
        nameMap.set("textInput", "Text Input");
        nameMap.set("timeupdate", "Time Update");
        nameMap.set("tonechange", "Tone Change");
        nameMap.set("touchcancel", "Touch Cancel");
        nameMap.set("touchend", "Touch End");
        nameMap.set("touchmove", "Touch Move");
        nameMap.set("touchstart", "Touch Start");
        nameMap.set("transitionend", "Transition End");
        nameMap.set("updateend", "Update End");
        nameMap.set("updateready", "Update Ready");
        nameMap.set("updatestart", "Update Start");
        nameMap.set("upgradeneeded", "Upgrade Needed");
        nameMap.set("versionchange", "Version Change");
        nameMap.set("visibilitychange", "Visibility Change");
        nameMap.set("volumechange", "Volume Change");
        nameMap.set("webglcontextcreationerror", "WebGL Context Creation Error");
        nameMap.set("webglcontextlost", "WebGL Context Lost");
        nameMap.set("webglcontextrestored", "WebGL Context Restored");
        nameMap.set("webkitAnimationEnd", "Animation End");
        nameMap.set("webkitAnimationIteration", "Animation Iteration");
        nameMap.set("webkitAnimationStart", "Animation Start");
        nameMap.set("webkitBeforeTextInserted", "Before Text Inserted");
        nameMap.set("webkitEditableContentChanged", "Editable Content Changed");
        nameMap.set("webkitTransitionEnd", "Transition End");
        nameMap.set("webkitaddsourcebuffer", "Add Source Buffer");
        nameMap.set("webkitbeginfullscreen", "Begin Full-Screen");
        nameMap.set("webkitcurrentplaybacktargetiswirelesschanged", "Current Playback Target Is Wireless Changed");
        nameMap.set("webkitendfullscreen", "End Full-Screen");
        nameMap.set("webkitfullscreenchange", "Full-Screen Change");
        nameMap.set("webkitfullscreenerror", "Full-Screen Error");
        nameMap.set("webkitkeyadded", "Key Added");
        nameMap.set("webkitkeyerror", "Key Error");
        nameMap.set("webkitkeymessage", "Key Message");
        nameMap.set("webkitneedkey", "Need Key");
        nameMap.set("webkitnetworkinfochange", "Network Info Change");
        nameMap.set("webkitplaybacktargetavailabilitychanged", "Playback Target Availability Changed");
        nameMap.set("webkitpointerlockchange", "Pointer Lock Change");
        nameMap.set("webkitpointerlockerror", "Pointer Lock Error");
        nameMap.set("webkitregionoversetchange", "Region Overset Change");
        nameMap.set("webkitremovesourcebuffer", "Remove Source Buffer");
        nameMap.set("webkitresourcetimingbufferfull", "Resource Timing Buffer Full");
        nameMap.set("webkitsourceclose", "Source Close");
        nameMap.set("webkitsourceended", "Source Ended");
        nameMap.set("webkitsourceopen", "Source Open");
        nameMap.set("webkitspeechchange", "Speech Change");
        nameMap.set("writeend", "Write End");
        nameMap.set("writestart", "Write Start");

        WI.ScriptTimelineRecord._eventDisplayNames = nameMap;
    }

    switch (eventType) {
    case WI.ScriptTimelineRecord.EventType.ScriptEvaluated:
    case WI.ScriptTimelineRecord.EventType.APIScriptEvaluated:
        return WI.UIString("Script Evaluated");
    case WI.ScriptTimelineRecord.EventType.MicrotaskDispatched:
        return WI.UIString("Microtask Dispatched");
    case WI.ScriptTimelineRecord.EventType.EventDispatched:
        if (details && (details instanceof String || typeof details === "string")) {
            var eventDisplayName = WI.ScriptTimelineRecord._eventDisplayNames.get(details) || details.capitalize();
            return WI.UIString("%s Event Dispatched").format(eventDisplayName);
        }
        return WI.UIString("Event Dispatched");
    case WI.ScriptTimelineRecord.EventType.ProbeSampleRecorded:
        return WI.UIString("Probe Sample Recorded");
    case WI.ScriptTimelineRecord.EventType.ConsoleProfileRecorded:
        if (details && (details instanceof String || typeof details === "string"))
            return WI.UIString("“%s” Profile Recorded").format(details);
        return WI.UIString("Console Profile Recorded");
    case WI.ScriptTimelineRecord.EventType.GarbageCollected:
        console.assert(details);
        if (details && (details instanceof WI.GarbageCollection) && includeDetailsInMainTitle) {
            switch (details.type) {
            case WI.GarbageCollection.Type.Partial:
                return WI.UIString("Partial Garbage Collection");
            case WI.GarbageCollection.Type.Full:
                return WI.UIString("Full Garbage Collection");
            }
        }
        return WI.UIString("Garbage Collection");
    case WI.ScriptTimelineRecord.EventType.TimerFired:
        if (details && includeDetailsInMainTitle)
            return WI.UIString("Timer %d Fired").format(details);
        return WI.UIString("Timer Fired");
    case WI.ScriptTimelineRecord.EventType.TimerInstalled:
        if (details && includeDetailsInMainTitle)
            return WI.UIString("Timer %d Installed").format(details.timerId);
        return WI.UIString("Timer Installed");
    case WI.ScriptTimelineRecord.EventType.TimerRemoved:
        if (details && includeDetailsInMainTitle)
            return WI.UIString("Timer %d Removed").format(details);
        return WI.UIString("Timer Removed");
    case WI.ScriptTimelineRecord.EventType.AnimationFrameFired:
        if (details && includeDetailsInMainTitle)
            return WI.UIString("Animation Frame %d Fired").format(details);
        return WI.UIString("Animation Frame Fired");
    case WI.ScriptTimelineRecord.EventType.AnimationFrameRequested:
        if (details && includeDetailsInMainTitle)
            return WI.UIString("Animation Frame %d Requested").format(details);
        return WI.UIString("Animation Frame Requested");
    case WI.ScriptTimelineRecord.EventType.AnimationFrameCanceled:
        if (details && includeDetailsInMainTitle)
            return WI.UIString("Animation Frame %d Canceled").format(details);
        return WI.UIString("Animation Frame Canceled");
    }
};

WI.ScriptTimelineRecord.TypeIdentifier = "script-timeline-record";
WI.ScriptTimelineRecord.EventTypeCookieKey = "script-timeline-record-event-type";
WI.ScriptTimelineRecord.DetailsCookieKey = "script-timeline-record-details";
