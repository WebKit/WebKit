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

// This function is invoked after the inspector has loaded and has a backend target.
WI.runBootstrapOperations = function() {
    WI.showDebugUISetting = new WI.Setting("show-debug-ui", false);

    // Toggle Debug UI setting.
    new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.Option | WI.KeyboardShortcut.Modifier.Shift | WI.KeyboardShortcut.Modifier.CommandOrControl, "D", () => {
        WI.showDebugUISetting.value = !WI.showDebugUISetting.value;
    });

    // Reload the Web Inspector.
    new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.Option | WI.KeyboardShortcut.Modifier.Shift | WI.KeyboardShortcut.Modifier.CommandOrControl, "R", () => {
        window.location.reload();
    });

    const dumpMessagesToolTip = WI.unlocalizedString("Enable dump inspector messages to console");
    const dumpMessagesActivatedToolTip = WI.unlocalizedString("Disable dump inspector messages to console");
    let dumpMessagesToolbarItem = new WI.ActivateButtonToolbarItem("dump-messages", dumpMessagesToolTip, dumpMessagesActivatedToolTip, "Images/Console.svg");
    dumpMessagesToolbarItem.activated = InspectorBackend.dumpInspectorProtocolMessages;
    dumpMessagesToolbarItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, () => {
        InspectorBackend.dumpInspectorProtocolMessages = !InspectorBackend.dumpInspectorProtocolMessages;
        dumpMessagesToolbarItem.activated = InspectorBackend.dumpInspectorProtocolMessages;
    });
    WI.settings.autoLogProtocolMessages.addEventListener(WI.Setting.Event.Changed, () => {
        dumpMessagesToolbarItem.activated = InspectorBackend.dumpInspectorProtocolMessages;
    });
    WI.toolbar.addToolbarItem(dumpMessagesToolbarItem, WI.Toolbar.Section.CenterRight);

    let inspectionLevel = InspectorFrontendHost ? InspectorFrontendHost.inspectionLevel() : 1;
    const inspectInspectorToolTip = WI.unlocalizedString("Open Web Inspector [%d]").format(inspectionLevel + 1);
    let inspectInspectorToolbarItem = new WI.ButtonToolbarItem("inspect-inspector", inspectInspectorToolTip);
    inspectInspectorToolbarItem.element.textContent = inspectionLevel + 1;
    inspectInspectorToolbarItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, () => {
        InspectorFrontendHost.inspectInspector();
    });
    WI.toolbar.addToolbarItem(inspectInspectorToolbarItem, WI.Toolbar.Section.CenterRight);

    function updateDebugUI()
    {
        dumpMessagesToolbarItem.hidden = !WI.showDebugUISetting.value;
        inspectInspectorToolbarItem.hidden = !WI.showDebugUISetting.value;
    }

    WI.showDebugUISetting.addEventListener(WI.Setting.Event.Changed, () => {
        updateDebugUI();
        WI.notifications.dispatchEventToListeners(WI.Notification.DebugUIEnabledDidChange);
    });

    updateDebugUI();
};
