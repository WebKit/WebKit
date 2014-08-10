/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

WebInspector.DebuggerDashboardView = function(representedObject)
{
    WebInspector.DashboardView.call(this, representedObject, "debugger");

    WebInspector.debuggerManager.addEventListener(WebInspector.DebuggerManager.Event.ActiveCallFrameDidChange, this._rebuildLocation, this);

    this._navigationBar = new WebInspector.NavigationBar;
    this.element.appendChild(this._navigationBar.element);

    var resumeImage;
    if (WebInspector.Platform.isLegacyMacOS)
        resumeImage = {src: "Images/Legacy/Resume.svg", width: 16, height: 16};
    else
        resumeImage = {src: "Images/Resume.svg", width: 15, height: 15};

    var tooltip = WebInspector.UIString("Continue script execution (%s or %s)").format(WebInspector.debuggerSidebarPanel.pauseOrResumeKeyboardShortcut.displayName, WebInspector.debuggerSidebarPanel.pauseOrResumeAlternateKeyboardShortcut.displayName);
    this._debuggerResumeButtonItem = new WebInspector.ActivateButtonNavigationItem("debugger-dashboard-pause", tooltip, tooltip, resumeImage.src, resumeImage.width, resumeImage.height, true);
    this._debuggerResumeButtonItem.activated = true;
    this._debuggerResumeButtonItem.addEventListener(WebInspector.ButtonNavigationItem.Event.Clicked, this._resumeButtonClicked, this);
    this._navigationBar.addNavigationItem(this._debuggerResumeButtonItem);

    var message = this._messageElement = document.createElement("div");
    message.classList.add(WebInspector.DebuggerDashboardView.MessageStyleClassName);
    message.title = message.textContent = WebInspector.UIString("Debugger Paused");
    this.element.appendChild(message);

    var dividerElement = document.createElement("div");
    dividerElement.classList.add(WebInspector.DebuggerDashboardView.DividerStyleClassName);
    this.element.appendChild(dividerElement);

    var locationElement = this._locationElement = document.createElement("div");
    locationElement.classList.add(WebInspector.DebuggerDashboardView.LocationStyleClassName);
    this.element.appendChild(locationElement);

    this._rebuildLocation();
};

WebInspector.DebuggerDashboardView.FunctionIconStyleClassName = WebInspector.CallFrameTreeElement.FunctionIconStyleClassName;
WebInspector.DebuggerDashboardView.EventListenerIconStyleClassName = WebInspector.CallFrameTreeElement.EventListenerIconStyleClassName;

WebInspector.DebuggerDashboardView.IconStyleClassName = "icon";
WebInspector.DebuggerDashboardView.MessageStyleClassName = "message";
WebInspector.DebuggerDashboardView.DividerStyleClassName = "divider";
WebInspector.DebuggerDashboardView.LocationStyleClassName = "location";
WebInspector.DebuggerDashboardView.FunctionNameStyleClassName = "function-name";

WebInspector.DebuggerDashboardView.prototype = {
    constructor: WebInspector.DebuggerDashboardView,
    __proto__: WebInspector.DashboardView.prototype,

    _rebuildLocation: function()
    {
        if (!WebInspector.debuggerManager.activeCallFrame)
            return;

        this._locationElement.removeChildren();

        var callFrame = WebInspector.debuggerManager.activeCallFrame;
        var functionName = callFrame.functionName || WebInspector.UIString("(anonymous function)");

        var iconClassName = WebInspector.DebuggerDashboardView.FunctionIconStyleClassName;

        // This is more than likely an event listener function with an "on" prefix and it is
        // as long or longer than the shortest event listener name -- "oncut".
        if (callFrame.functionName && callFrame.functionName.startsWith("on") && callFrame.functionName.length >= 5)
            iconClassName = WebInspector.DebuggerDashboardView.EventListenerIconStyleClassName;

        var iconElement = document.createElement("div");
        iconElement.classList.add(iconClassName);
        this._locationElement.appendChild(iconElement);

        var iconImageElement = document.createElement("img");
        iconImageElement.className = WebInspector.DebuggerDashboardView.IconStyleClassName;
        iconElement.appendChild(iconImageElement);

        var nameElement = document.createElement("div");
        nameElement.appendChild(document.createTextNode(functionName));
        nameElement.classList.add(WebInspector.DebuggerDashboardView.FunctionNameStyleClassName);
        this._locationElement.appendChild(nameElement);

        var sourceCodeLocation = WebInspector.debuggerManager.activeCallFrame.sourceCodeLocation;
        var shouldPreventLinkFloat = true;
        var linkElement = WebInspector.createSourceCodeLocationLink(sourceCodeLocation, shouldPreventLinkFloat);
        this._locationElement.appendChild(linkElement);
    },

    _resumeButtonClicked: function()
    {
        WebInspector.debuggerManager.resume();
    }
};
