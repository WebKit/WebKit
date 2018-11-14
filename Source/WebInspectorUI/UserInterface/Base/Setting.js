/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

WI.Setting = class Setting extends WI.Object
{
    constructor(name, defaultValue)
    {
        super();

        this._name = name;

        let inspectionLevel = InspectorFrontendHost ? InspectorFrontendHost.inspectionLevel() : 1;
        let levelString = inspectionLevel > 1 ? "-" + inspectionLevel : "";
        this._localStorageKey = `com.apple.WebInspector${levelString}.${name}`;
        this._defaultValue = defaultValue;
    }

    // Public

    get name()
    {
        return this._name;
    }

    get value()
    {
        if ("_value" in this)
            return this._value;

        // Make a copy of the default value so changes to object values don't modify the default value.
        this._value = JSON.parse(JSON.stringify(this._defaultValue));

        if (!window.InspectorTest && window.localStorage && this._localStorageKey in window.localStorage) {
            try {
                this._value = JSON.parse(window.localStorage[this._localStorageKey]);
            } catch {
                delete window.localStorage[this._localStorageKey];
            }
        }

        return this._value;
    }

    set value(value)
    {
        if (this._value === value)
            return;

        this._value = value;

        if (!window.InspectorTest && window.localStorage) {
            try {
                // Use Object.shallowEqual to properly compare objects.
                if (Object.shallowEqual(this._value, this._defaultValue))
                    delete window.localStorage[this._localStorageKey];
                else
                    window.localStorage[this._localStorageKey] = JSON.stringify(this._value);
            } catch {
                console.error("Error saving setting with name: " + this._name);
            }
        }

        this.dispatchEventToListeners(WI.Setting.Event.Changed, this._value, {name: this._name});
    }

    reset()
    {
        // Make a copy of the default value so changes to object values don't modify the default value.
        this.value = JSON.parse(JSON.stringify(this._defaultValue));
    }
};

WI.Setting.Event = {
    Changed: "setting-changed"
};

WI.settings = {
    canvasRecordingAutoCaptureEnabled: new WI.Setting("canvas-recording-auto-capture-enabled", false),
    canvasRecordingAutoCaptureFrameCount: new WI.Setting("canvas-recording-auto-capture-frame-count", 1),
    clearLogOnNavigate: new WI.Setting("clear-log-on-navigate", true),
    clearNetworkOnNavigate: new WI.Setting("clear-network-on-navigate", true),
    enableControlFlowProfiler: new WI.Setting("enable-control-flow-profiler", false),
    enableLineWrapping: new WI.Setting("enable-line-wrapping", false),
    groupByDOMNode: new WI.Setting("group-by-dom-node", false),
    indentUnit: new WI.Setting("indent-unit", 4),
    indentWithTabs: new WI.Setting("indent-with-tabs", false),
    resourceCachingDisabled: new WI.Setting("disable-resource-caching", false),
    selectedNetworkDetailContentViewIdentifier: new WI.Setting("network-detail-content-view-identifier", "preview"),
    showAllRequestsBreakpoint: new WI.Setting("show-all-requests-breakpoint", true),
    showAssertionFailuresBreakpoint: new WI.Setting("show-assertion-failures-breakpoint", true),
    showCanvasPath: new WI.Setting("show-canvas-path", false),
    showImageGrid: new WI.Setting("show-image-grid", false),
    showInvalidCharacters: new WI.Setting("show-invalid-characters", false),
    showJavaScriptTypeInformation: new WI.Setting("show-javascript-type-information", false),
    showPaintRects: new WI.Setting("show-paint-rects", false),
    showRulers: new WI.Setting("show-rulers", false),
    showScopeChainOnPause: new WI.Setting("show-scope-chain-sidebar", true),
    showShadowDOM: new WI.Setting("show-shadow-dom", false),
    showWhitespaceCharacters: new WI.Setting("show-whitespace-characters", false),
    tabSize: new WI.Setting("tab-size", 4),
    zoomFactor: new WI.Setting("zoom-factor", 1),

    // Experimental
    experimentalEnableMultiplePropertiesSelection: new WI.Setting("experimental-enable-multiple-properties-selection", false),
    experimentalEnableLayersTab: new WI.Setting("experimental-enable-layers-tab", false),
    experimentalEnableNewTabBar: new WI.Setting("experimental-enable-new-tab-bar", false),
    experimentalEnableAuditTab: new WI.Setting("experimental-enable-audit-tab", false),

    // DebugUI
    autoLogProtocolMessages: new WI.Setting("auto-collect-protocol-messages", false),
    autoLogTimeStats: new WI.Setting("auto-collect-time-stats", false),
    enableLayoutFlashing: new WI.Setting("enable-layout-flashing", false),
    enableUncaughtExceptionReporter: new WI.Setting("enable-uncaught-exception-reporter", true),
    filterMultiplexingBackendInspectorProtocolMessages: new WI.Setting("filter-multiplexing-backend-inspector-protocol-messages", true),
    layoutDirection: new WI.Setting("layout-direction-override", "system"),
    pauseForInternalScripts: new WI.Setting("pause-for-internal-scripts", false),
};
