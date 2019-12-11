/*
 * Copyright (C) 2014-2015 Apple Inc. All rights reserved.
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

WI.DebuggerDashboardView = class DebuggerDashboardView extends WI.DashboardView
{
    constructor(representedObject)
    {
        super(representedObject, "debugger");

        WI.debuggerManager.addEventListener(WI.DebuggerManager.Event.ActiveCallFrameDidChange, this._rebuildLocation, this);

        this._navigationBar = new WI.NavigationBar;
        this.element.appendChild(this._navigationBar.element);

        var tooltip = WI.UIString("Continue script execution (%s or %s)").format(WI.pauseOrResumeKeyboardShortcut.displayName, WI.pauseOrResumeAlternateKeyboardShortcut.displayName);
        this._debuggerResumeButtonItem = new WI.ActivateButtonNavigationItem("debugger-dashboard-pause", tooltip, tooltip, "Images/Resume.svg", 15, 15);
        this._debuggerResumeButtonItem.activated = true;
        this._debuggerResumeButtonItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, this._resumeButtonClicked, this);
        this._navigationBar.addNavigationItem(this._debuggerResumeButtonItem);

        var message = this._messageElement = document.createElement("div");
        message.classList.add("message");
        message.title = message.textContent = WI.UIString("Debugger Paused");
        this.element.appendChild(message);

        var dividerElement = document.createElement("div");
        dividerElement.classList.add("divider");
        this.element.appendChild(dividerElement);

        var locationElement = this._locationElement = document.createElement("div");
        locationElement.classList.add("location");
        this.element.appendChild(locationElement);

        this._rebuildLocation();
    }

    // Public

    closed()
    {
        WI.debuggerManager.removeEventListener(null, null, this);

        super.closed();
    }

    // Private

    _rebuildLocation()
    {
        if (!WI.debuggerManager.activeCallFrame)
            return;

        this._locationElement.removeChildren();

        var callFrame = WI.debuggerManager.activeCallFrame;
        var functionName = callFrame.functionName || WI.UIString("(anonymous function)");

        var iconClassName = WI.DebuggerDashboardView.FunctionIconStyleClassName;

        // This is more than likely an event listener function with an "on" prefix and it is
        // as long or longer than the shortest event listener name -- "oncut".
        if (callFrame.functionName && callFrame.functionName.startsWith("on") && callFrame.functionName.length >= 5)
            iconClassName = WI.DebuggerDashboardView.EventListenerIconStyleClassName;

        var iconElement = document.createElement("div");
        iconElement.classList.add(iconClassName);
        this._locationElement.appendChild(iconElement);

        var iconImageElement = document.createElement("img");
        iconImageElement.className = "icon";
        iconElement.appendChild(iconImageElement);

        var nameElement = document.createElement("div");
        nameElement.append(functionName);
        nameElement.classList.add("function-name");
        this._locationElement.appendChild(nameElement);

        const options = {
            dontFloat: true,
            ignoreNetworkTab: true,
            ignoreSearchTab: true,
        };
        let linkElement = WI.createSourceCodeLocationLink(WI.debuggerManager.activeCallFrame.sourceCodeLocation, options);
        this._locationElement.appendChild(linkElement);
    }

    _resumeButtonClicked()
    {
        WI.debuggerManager.resume();
    }
};

WI.DebuggerDashboardView.FunctionIconStyleClassName = WI.CallFrameView.FunctionIconStyleClassName;
WI.DebuggerDashboardView.EventListenerIconStyleClassName = WI.CallFrameView.EventListenerIconStyleClassName;
