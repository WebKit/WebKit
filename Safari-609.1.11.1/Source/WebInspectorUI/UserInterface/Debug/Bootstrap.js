/*
 * Copyright (C) 2015 University of Washington.
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

WI.isEngineeringBuild = true;

// Disable Pause in Internal Scripts if Show Internal Scripts is dibbled.
WI.settings.engineeringShowInternalScripts.addEventListener(WI.Setting.Event.Changed, (event) => {
    if (!WI.settings.engineeringShowInternalScripts.value)
        WI.settings.engineeringPauseForInternalScripts.value = false;
}, WI.settings.engineeringPauseForInternalScripts);

// Enable Show Internal Scripts if Pause in Internal Scripts is enabled.
WI.settings.engineeringPauseForInternalScripts.addEventListener(WI.Setting.Event.Changed, (event) => {
    if (WI.settings.engineeringPauseForInternalScripts.value)
        WI.settings.engineeringShowInternalScripts.value = true;
}, WI.settings.engineeringShowInternalScripts);

WI.showDebugUISetting = new WI.Setting("show-debug-ui", false);

// This function is invoked after the inspector has loaded and has a backend target.
WI.runBootstrapOperations = function() {
    // Toggle Debug UI setting.
    new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.Option | WI.KeyboardShortcut.Modifier.Shift | WI.KeyboardShortcut.Modifier.CommandOrControl, "D", () => {
        WI.showDebugUISetting.value = !WI.showDebugUISetting.value;
    });

    // Reload the Web Inspector.
    new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.Option | WI.KeyboardShortcut.Modifier.Shift | WI.KeyboardShortcut.Modifier.CommandOrControl, "R", () => {
        InspectorFrontendHost.reopen();
    });

    // Toggle Inspector Messages Filtering.
    let ignoreChangesToState = false;
    const DumpMessagesState = {Off: "off", Filtering: "filtering", Everything: "everything"};
    const dumpMessagesToolTip = WI.unlocalizedString("Enable dump inspector messages to console.\nShift-click to dump all inspector messages with no filtering.");
    const dumpMessagesActivatedToolTip = WI.unlocalizedString("Disable dump inspector messages to console");
    let dumpMessagesToolbarItem = new WI.ActivateButtonToolbarItem("dump-messages", dumpMessagesToolTip, dumpMessagesActivatedToolTip, "Images/Console.svg");

    function dumpMessagesCurrentState() {
        if (!InspectorBackend.dumpInspectorProtocolMessages)
            return DumpMessagesState.Off;
        if (InspectorBackend.filterMultiplexingBackendInspectorProtocolMessages)
            return DumpMessagesState.Filtering;
        return DumpMessagesState.Everything;
    }

    function applyDumpMessagesState(state) {
        ignoreChangesToState = true;
        switch (state) {
        case DumpMessagesState.Off:
            InspectorBackend.dumpInspectorProtocolMessages = false;
            InspectorBackend.filterMultiplexingBackendInspectorProtocolMessages = false;
            dumpMessagesToolbarItem.activated = false;
            dumpMessagesToolbarItem.element.style.removeProperty("color");
            break;
        case DumpMessagesState.Filtering:
            InspectorBackend.dumpInspectorProtocolMessages = true;
            InspectorBackend.filterMultiplexingBackendInspectorProtocolMessages = true;
            dumpMessagesToolbarItem.activated = true;
            dumpMessagesToolbarItem.element.style.removeProperty("color");
            break;
        case DumpMessagesState.Everything:
            InspectorBackend.dumpInspectorProtocolMessages = true;
            InspectorBackend.filterMultiplexingBackendInspectorProtocolMessages = false;
            dumpMessagesToolbarItem.activated = true;
            dumpMessagesToolbarItem.element.style.color = "rgb(164, 41, 154)";
            break;
        }
        ignoreChangesToState = false;
    }

    WI.toolbar.addToolbarItem(dumpMessagesToolbarItem, WI.Toolbar.Section.CenterRight);
    dumpMessagesToolbarItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, () => {
        let nextState;
        switch (dumpMessagesCurrentState()) {
        case DumpMessagesState.Off:
            nextState = WI.modifierKeys.shiftKey ? DumpMessagesState.Everything : DumpMessagesState.Filtering;
            break;
        case DumpMessagesState.Filtering:
            nextState = WI.modifierKeys.shiftKey ? DumpMessagesState.Everything : DumpMessagesState.Off;
            break;
        case DumpMessagesState.Everything:
            nextState = DumpMessagesState.Off;
            break;
        }
        applyDumpMessagesState(nextState);
    });
    WI.settings.protocolAutoLogMessages.addEventListener(WI.Setting.Event.Changed, () => {
        if (ignoreChangesToState)
            return;
        applyDumpMessagesState(dumpMessagesCurrentState());
    });
    applyDumpMessagesState(dumpMessagesCurrentState());

    // Next Level Inspector.
    let inspectionLevel = InspectorFrontendHost.inspectionLevel();
    const inspectInspectorToolTip = WI.unlocalizedString("Open Web Inspector [%d]").format(inspectionLevel + 1);
    let inspectInspectorToolbarItem = new WI.ButtonToolbarItem("inspect-inspector", inspectInspectorToolTip);
    WI.toolbar.addToolbarItem(inspectInspectorToolbarItem, WI.Toolbar.Section.CenterRight);
    inspectInspectorToolbarItem.element.textContent = inspectionLevel + 1;
    inspectInspectorToolbarItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, () => {
        InspectorFrontendHost.inspectInspector();
    });

    function updateDebugUI() {
        dumpMessagesToolbarItem.hidden = !WI.showDebugUISetting.value;
        inspectInspectorToolbarItem.hidden = !WI.showDebugUISetting.value;
    }

    WI.showDebugUISetting.addEventListener(WI.Setting.Event.Changed, () => {
        updateDebugUI();
    });

    updateDebugUI();
};
