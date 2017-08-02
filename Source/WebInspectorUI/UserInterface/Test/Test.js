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

WI.DebuggableType = {
    Web: "web",
    JavaScript: "javascript"
};

WI.loaded = function()
{
    this.debuggableType = WI.DebuggableType.Web;
    this.hasExtraDomains = false;

    // Register observers for events from the InspectorBackend.
    // The initialization order should match the same in Main.js.
    InspectorBackend.registerInspectorDispatcher(new WI.InspectorObserver);
    InspectorBackend.registerPageDispatcher(new WI.PageObserver);
    InspectorBackend.registerConsoleDispatcher(new WI.ConsoleObserver);
    InspectorBackend.registerDOMDispatcher(new WI.DOMObserver);
    InspectorBackend.registerNetworkDispatcher(new WI.NetworkObserver);
    InspectorBackend.registerDebuggerDispatcher(new WI.DebuggerObserver);
    InspectorBackend.registerHeapDispatcher(new WI.HeapObserver);
    InspectorBackend.registerDOMStorageDispatcher(new WI.DOMStorageObserver);
    InspectorBackend.registerTimelineDispatcher(new WI.TimelineObserver);
    InspectorBackend.registerCSSDispatcher(new WI.CSSObserver);
    InspectorBackend.registerRuntimeDispatcher(new WI.RuntimeObserver);
    InspectorBackend.registerWorkerDispatcher(new WI.WorkerObserver);
    InspectorBackend.registerCanvasDispatcher(new WI.CanvasObserver);

    WI.mainTarget = new WI.MainTarget;

    // Instantiate controllers used by tests.
    this.targetManager = new WI.TargetManager;
    this.frameResourceManager = new WI.FrameResourceManager;
    this.storageManager = new WI.StorageManager;
    this.domTreeManager = new WI.DOMTreeManager;
    this.cssStyleManager = new WI.CSSStyleManager;
    this.logManager = new WI.LogManager;
    this.issueManager = new WI.IssueManager;
    this.runtimeManager = new WI.RuntimeManager;
    this.heapManager = new WI.HeapManager;
    this.memoryManager = new WI.MemoryManager;
    this.timelineManager = new WI.TimelineManager;
    this.debuggerManager = new WI.DebuggerManager;
    this.probeManager = new WI.ProbeManager;
    this.workerManager = new WI.WorkerManager;
    this.domDebuggerManager = new WI.DOMDebuggerManager;
    this.canvasManager = new WI.CanvasManager;

    document.addEventListener("DOMContentLoaded", this.contentLoaded);

    // Enable agents.
    InspectorAgent.enable();
    ConsoleAgent.enable();

    // Perform one-time tasks.
    WI.CSSCompletions.requestCSSCompletions();

    // Global settings.
    this.showShadowDOMSetting = new WI.Setting("show-shadow-dom", true);
};

WI.contentLoaded = function()
{
    // Signal that the frontend is now ready to receive messages.
    InspectorFrontendAPI.loadCompleted();

    // Tell the InspectorFrontendHost we loaded, which causes the window to display
    // and pending InspectorFrontendAPI commands to be sent.
    InspectorFrontendHost.loaded();
};

Object.defineProperty(WI, "targets",
{
    get() { return this.targetManager.targets; }
});

WI.assumingMainTarget = () => WI.mainTarget;

WI.isDebugUIEnabled = () => false;

WI.unlocalizedString = (string) => string;
WI.UIString = (string) => string;

WI.indentString = () => "    ";

// Add stubs that are called by the frontend API.
WI.updateDockedState = () => {};
WI.updateDockingAvailability = () => {};
WI.updateVisibilityState = () => {};

window.InspectorTest = new FrontendTestHarness();

InspectorTest.redirectConsoleToTestOutput();

WI.reportInternalError = (e) => { console.error(e); };

window.reportUnhandledRejection = InspectorTest.reportUnhandledRejection.bind(InspectorTest);
window.onerror = InspectorTest.reportUncaughtExceptionFromEvent.bind(InspectorTest);
