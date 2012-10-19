/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

/**
 * @constructor
 */
WebInspector.DockController = function()
{
    this._dockToggleButton = new WebInspector.StatusBarButton("", "dock-status-bar-item", 3);
    this._dockToggleButtonOption = new WebInspector.StatusBarButton("", "dock-status-bar-item", 3);
    this._dockToggleButton.addEventListener("click", this._toggleDockState, this);
    this._dockToggleButtonOption.addEventListener("click", this._toggleDockState, this);
    if (Preferences.showDockToRight)
        this._dockToggleButton.makeLongClickEnabled(this._createDockOptions.bind(this));

    this._dockSide = WebInspector.queryParamsObject["dockSide"];
    this._innerSetDocked(WebInspector.queryParamsObject["docked"] === "true");
}

WebInspector.DockController.State = {
    DockedToBottom: "bottom",
    DockedToRight: "right",
    Undocked: "undocked"
}

WebInspector.DockController.prototype = {
    /**
     * @return {Element}
     */
    get element()
    {
        return this._dockToggleButton.element;
    },

    /**
     * @param {boolean} docked
     */
    setDocked: function(docked)
    {
        var isDocked = this._state !== WebInspector.DockController.State.Undocked;
        if (docked !== isDocked)
            this._innerSetDocked(docked);
    },

    /**
     * @param {boolean} docked
     */
    _innerSetDocked: function(docked)
    {
        if (this._state)
            WebInspector.settings.lastDockState.set(this._state);

        if (!docked) {
            this._state = WebInspector.DockController.State.Undocked;
            WebInspector.userMetrics.WindowDocked.record();
        } else if (this._dockSide === "right") {
            this._state = WebInspector.DockController.State.DockedToRight;
            WebInspector.userMetrics.WindowUndocked.record();
        } else {
            this._state = WebInspector.DockController.State.DockedToBottom;
            WebInspector.userMetrics.WindowUndocked.record();
        }
        this._updateUI();
    },

    /**
     * @param {boolean} unavailable
     */
    setDockingUnavailable: function(unavailable)
    {
        this._isDockingUnavailable = unavailable;
        this._updateUI();
    },

    _updateUI: function()
    {
        var body = document.body;
        switch (this._state) {
        case WebInspector.DockController.State.DockedToBottom:
            body.removeStyleClass("undocked");
            body.removeStyleClass("dock-to-right");
            this.setCompactMode(true);
            break;
        case WebInspector.DockController.State.DockedToRight: 
            body.removeStyleClass("undocked");
            body.addStyleClass("dock-to-right");
            this.setCompactMode(false);
            break;
        case WebInspector.DockController.State.Undocked: 
            body.addStyleClass("undocked");
            body.removeStyleClass("dock-to-right");
            this.setCompactMode(false);
            break;
        }

        if (this._isDockingUnavailable) {
            this._dockToggleButton.state = "undock";
            this._dockToggleButton.disabled = true;
            return;
        }

        this._dockToggleButton.disabled = false;

        // Choose different last state based on the current one if missing or if is the same.
        var states = [WebInspector.DockController.State.DockedToBottom, WebInspector.DockController.State.Undocked, WebInspector.DockController.State.DockedToRight];
        states.remove(this._state);
        var lastState = WebInspector.settings.lastDockState.get();

        states.remove(lastState);
        if (states.length === 2) { // last state was not from the list of potential values
            lastState = states[0];
            states.remove(lastState);
        }
        this._decorateButtonForTargetState(this._dockToggleButton, lastState);
        this._decorateButtonForTargetState(this._dockToggleButtonOption, states[0]);
    },

    /**
     * @param {WebInspector.StatusBarButton} button
     * @param {string} state
     */
    _decorateButtonForTargetState: function(button, state)
    {
        switch (state) {
        case WebInspector.DockController.State.DockedToBottom:
            button.title = WebInspector.UIString("Dock to main window.");
            button.state = "bottom";
            break;
        case WebInspector.DockController.State.DockedToRight:
            button.title = WebInspector.UIString("Dock to main window.");
            button.state = "right";
            break;
        case WebInspector.DockController.State.Undocked: 
            button.title = WebInspector.UIString("Undock into separate window.");
            button.state = "undock";
            break;
        }
    },

    _createDockOptions: function()
    {
        return [this._dockToggleButtonOption];
    },

    /**
     * @param {WebInspector.Event} e
     */
    _toggleDockState: function(e)
    {
        var state = e.target.state;
        switch (state) {
        case "undock":
            InspectorFrontendHost.requestDetachWindow();
            WebInspector.userMetrics.WindowUndocked.record();
            break;
        case "right":
        case "bottom":
            this._dockSide = state;
            InspectorFrontendHost.requestSetDockSide(this._dockSide);
            if (this._state === WebInspector.DockController.State.Undocked)
                InspectorFrontendHost.requestAttachWindow();
            else
                this._innerSetDocked(true);
            break;
        }
    },

    /**
     * @return {boolean}
     */
    isCompactMode: function()
    {
        return this._isCompactMode;
    },

    /**
     * @param {boolean} isCompactMode
     */
    setCompactMode: function(isCompactMode)
    {
        var body = document.body;
        this._isCompactMode = isCompactMode;
        if (WebInspector.toolbar)
            WebInspector.toolbar.setCompactMode(isCompactMode);
        if (isCompactMode)
            body.addStyleClass("compact");
        else
            body.removeStyleClass("compact");
    }
}
