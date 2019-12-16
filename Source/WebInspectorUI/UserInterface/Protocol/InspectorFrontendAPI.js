/*
 * Copyright (C) 2013, 2016 Apple Inc. All rights reserved.
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

InspectorFrontendAPI = {
    _loaded: false,
    _pendingCommands: [],

    savedURL: function(url)
    {
        // Not used yet.
    },

    appendedToURL: function(url)
    {
        // Not used yet.
    },

    isTimelineProfilingEnabled: function()
    {
        return WI.timelineManager.isCapturing();
    },

    setTimelineProfilingEnabled: function(enabled)
    {
        if (!WI.targetsAvailable()) {
            WI.whenTargetsAvailable().then(() => {
                InspectorFrontendAPI.setTimelineProfilingEnabled(enabled);
            });
            return;
        }

        WI.showTimelineTab({
            initiatorHint: WI.TabBrowser.TabNavigationInitiator.FrontendAPI
        });

        if (WI.timelineManager.isCapturing() === enabled)
            return;

        if (enabled)
            WI.timelineManager.startCapturing();
        else
            WI.timelineManager.stopCapturing();
    },

    setElementSelectionEnabled: function(enabled)
    {
        if (!WI.targetsAvailable()) {
            WI.whenTargetsAvailable().then(() => {
                InspectorFrontendAPI.setElementSelectionEnabled(enabled);
            });
            return;
        }

        WI.domManager.inspectModeEnabled = enabled;
    },

    setDockingUnavailable: function(unavailable)
    {
        WI.updateDockingAvailability(!unavailable);
    },

    setDockSide: function(side)
    {
        WI.updateDockedState(side);
    },

    setIsVisible: function(visible)
    {
        WI.updateVisibilityState(visible);
    },

    setDiagnosticLoggingAvailable: function(available)
    {
        if (WI.diagnosticController)
            WI.diagnosticController.diagnosticLoggingAvailable = available;
    },

    showConsole: function()
    {
        const requestedScope = null;
        WI.showConsoleTab(requestedScope, {
            initiatorHint: WI.TabBrowser.TabNavigationInitiator.FrontendAPI,
        });

        WI.quickConsole.prompt.focus();

        // If the page is still loading, focus the quick console again after tabindex autofocus.
        if (document.readyState !== "complete")
            document.addEventListener("readystatechange", this);
        if (document.visibilityState !== "visible")
            document.addEventListener("visibilitychange", this);
    },

    handleEvent: function(event)
    {
        console.assert(event.type === "readystatechange" || event.type === "visibilitychange");

        if (document.readyState === "complete" && document.visibilityState === "visible") {
            WI.quickConsole.prompt.focus();
            document.removeEventListener("readystatechange", this);
            document.removeEventListener("visibilitychange", this);
        }
    },

    showResources: function()
    {
        WI.showSourcesTab({
            initiatorHint: WI.TabBrowser.TabNavigationInitiator.FrontendAPI,
        });
    },

    // COMPATIBILITY (iOS 13): merged into InspectorFrontendAPI.setTimelineProfilingEnabled.
    showTimelines: function()
    {
        WI.showTimelineTab({
            initiatorHint: WI.TabBrowser.TabNavigationInitiator.FrontendAPI
        });
    },

    showMainResourceForFrame: function(frameIdentifier)
    {
        WI.showSourceCodeForFrame(frameIdentifier, {
            ignoreNetworkTab: true,
            ignoreSearchTab: true,
            initiatorHint: WI.TabBrowser.TabNavigationInitiator.FrontendAPI,
        });
    },

    contextMenuItemSelected: function(id)
    {
        WI.ContextMenu.contextMenuItemSelected(id);
    },

    contextMenuCleared: function()
    {
        WI.ContextMenu.contextMenuCleared();
    },

    dispatchMessageAsync: function(messageObject)
    {
        WI.dispatchMessageFromBackend(messageObject);
    },

    dispatchMessage: function(messageObject)
    {
        InspectorBackend.dispatch(messageObject);
    },

    dispatch: function(signature)
    {
        if (!InspectorFrontendAPI._loaded) {
            InspectorFrontendAPI._pendingCommands.push(signature);
            return null;
        }

        var methodName = signature.shift();
        console.assert(InspectorFrontendAPI[methodName], "Unexpected InspectorFrontendAPI method name: " + methodName);
        if (!InspectorFrontendAPI[methodName])
            return null;

        return InspectorFrontendAPI[methodName].apply(InspectorFrontendAPI, signature);
    },

    loadCompleted: function()
    {
        InspectorFrontendAPI._loaded = true;

        for (var i = 0; i < InspectorFrontendAPI._pendingCommands.length; ++i)
            InspectorFrontendAPI.dispatch(InspectorFrontendAPI._pendingCommands[i]);

        delete InspectorFrontendAPI._pendingCommands;
    }
};
