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

// This function is invoked after the inspector has loaded.
WebInspector.runBootstrapOperations = function() {
    WebInspector.showDebugUISetting = new WebInspector.Setting("show-debug-ui", false);

    // Toggle Debug UI setting.
    new WebInspector.KeyboardShortcut(WebInspector.KeyboardShortcut.Modifier.Option | WebInspector.KeyboardShortcut.Modifier.Shift | WebInspector.KeyboardShortcut.Modifier.CommandOrControl, "D", () => {
        WebInspector.showDebugUISetting.value = !WebInspector.showDebugUISetting.value;
    });

    // Reload the Web Inspector.
    new WebInspector.KeyboardShortcut(WebInspector.KeyboardShortcut.Modifier.Option | WebInspector.KeyboardShortcut.Modifier.Shift | WebInspector.KeyboardShortcut.Modifier.CommandOrControl, "R", () => {
        window.location.reload();
    });

    let toolTip = "Enable dump inspector messages to console";
    let activatedToolTip = "Disable dump inspector messages to console";
    let debugInspectorToolbarButton = new WebInspector.ActivateButtonToolbarItem("debug-inspector", toolTip, activatedToolTip, null, "Images/Console.svg");
    debugInspectorToolbarButton.activated = InspectorBackend.dumpInspectorProtocolMessages;
    WebInspector.toolbar.addToolbarItem(debugInspectorToolbarButton, WebInspector.Toolbar.Section.CenterRight);
    debugInspectorToolbarButton.addEventListener(WebInspector.ButtonNavigationItem.Event.Clicked, () => {
        InspectorBackend.dumpInspectorProtocolMessages = !InspectorBackend.dumpInspectorProtocolMessages;
        debugInspectorToolbarButton.activated = InspectorBackend.dumpInspectorProtocolMessages;
    });

    function updateDebugUI()
    {
        debugInspectorToolbarButton.hidden = !WebInspector.showDebugUISetting.value;
    }

    WebInspector.showDebugUISetting.addEventListener(WebInspector.Setting.Event.Changed, () => {
        updateDebugUI();
        WebInspector.notifications.dispatchEventToListeners(WebInspector.Notification.DebugUIEnabledDidChange);
    });

    updateDebugUI();
};
