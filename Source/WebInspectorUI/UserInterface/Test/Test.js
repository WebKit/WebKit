/*
 * Copyright (C) 2013-2015 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

WebInspector.DebuggableType = {
    Web: "web",
    JavaScript: "javascript"
};

WebInspector.loaded = function()
{
    this.debuggableType = WebInspector.DebuggableType.Web;
    this.hasExtraDomains = false;

    // Register observers for events from the InspectorBackend.
    // The initialization order should match the same in Main.js.
    InspectorBackend.registerInspectorDispatcher(new WebInspector.InspectorObserver);
    InspectorBackend.registerPageDispatcher(new WebInspector.PageObserver);
    InspectorBackend.registerConsoleDispatcher(new WebInspector.ConsoleObserver);
    InspectorBackend.registerDOMDispatcher(new WebInspector.DOMObserver);
    InspectorBackend.registerNetworkDispatcher(new WebInspector.NetworkObserver);
    InspectorBackend.registerDebuggerDispatcher(new WebInspector.DebuggerObserver);
    InspectorBackend.registerHeapDispatcher(new WebInspector.HeapObserver);
    InspectorBackend.registerDOMStorageDispatcher(new WebInspector.DOMStorageObserver);
    InspectorBackend.registerTimelineDispatcher(new WebInspector.TimelineObserver);
    InspectorBackend.registerCSSDispatcher(new WebInspector.CSSObserver);
    InspectorBackend.registerRuntimeDispatcher(new WebInspector.RuntimeObserver);
    if (InspectorBackend.registerReplayDispatcher)
        InspectorBackend.registerReplayDispatcher(new WebInspector.ReplayObserver);

    // Instantiate controllers used by tests.
    this.frameResourceManager = new WebInspector.FrameResourceManager;
    this.storageManager = new WebInspector.StorageManager;
    this.domTreeManager = new WebInspector.DOMTreeManager;
    this.cssStyleManager = new WebInspector.CSSStyleManager;
    this.logManager = new WebInspector.LogManager;
    this.issueManager = new WebInspector.IssueManager;
    this.runtimeManager = new WebInspector.RuntimeManager;
    this.heapManager = new WebInspector.HeapManager;
    this.memoryManager = new WebInspector.MemoryManager;
    this.timelineManager = new WebInspector.TimelineManager;
    this.debuggerManager = new WebInspector.DebuggerManager;
    this.probeManager = new WebInspector.ProbeManager;
    this.replayManager = new WebInspector.ReplayManager;

    document.addEventListener("DOMContentLoaded", this.contentLoaded);

    // Enable agents.
    InspectorAgent.enable();
    ConsoleAgent.enable();

    // Perform one-time tasks.
    WebInspector.CSSCompletions.requestCSSCompletions();

    // Global settings.
    this.showShadowDOMSetting = new WebInspector.Setting("show-shadow-dom", true);
}

WebInspector.contentLoaded = function()
{
    // Signal that the frontend is now ready to receive messages.
    InspectorFrontendAPI.loadCompleted();

    // Tell the InspectorFrontendHost we loaded, which causes the window to display
    // and pending InspectorFrontendAPI commands to be sent.
    InspectorFrontendHost.loaded();
}

WebInspector.isDebugUIEnabled = () => false;

WebInspector.UIString = (string) => string;

// Add stubs that are called by the frontend API.
WebInspector.updateDockedState = () => {};
WebInspector.updateDockingAvailability = () => {};
WebInspector.updateVisibilityState = () => {};

window.InspectorTest = new FrontendTestHarness();

InspectorTest.redirectConsoleToTestOutput();
