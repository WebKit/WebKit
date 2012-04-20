/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. AND ITS CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE INC.
 * OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @constructor
 * @param {WebInspector.Panel} panel
 * @param {WebInspector.SplitView} parentSplitView
 * @param {WebInspector.View} navigatorView
 * @param {WebInspector.View} editorView
 */
WebInspector.NavigatorOverlayController = function(panel, parentSplitView, navigatorView, editorView)
{
    this._panel = panel;
    this._parentSplitView = parentSplitView;
    this._navigatorView = navigatorView;
    this._editorView = editorView;

    this._navigatorSidebarResizeWidgetElement = document.createElement("div");
    this._navigatorSidebarResizeWidgetElement.addStyleClass("scripts-navigator-resizer-widget");
    this._parentSplitView.installResizer(this._navigatorSidebarResizeWidgetElement);
    this._navigatorView.element.appendChild(this._navigatorSidebarResizeWidgetElement);

    this._navigatorShowHideButton = this._createNavigatorControlButton(WebInspector.UIString("Show navigator"), "scripts-navigator-show-hide-button", this._toggleNavigator.bind(this));
    this._navigatorShowHideButton.addStyleClass("toggled-on");
    this._navigatorShowHideButton.title = WebInspector.UIString("Hide navigator");
    this._parentSplitView.element.appendChild(this._navigatorShowHideButton);

    this._navigatorPinButton = this._createNavigatorControlButton(WebInspector.UIString("Pin navigator"), "scripts-navigator-pin-button", this._pinNavigator.bind(this));
    this._navigatorPinButton.addStyleClass("hidden");
    this._navigatorView.element.appendChild(this._navigatorPinButton);

    WebInspector.settings.navigatorHidden = WebInspector.settings.createSetting("navigatorHidden", true);
    if (WebInspector.settings.navigatorHidden.get())
        this._toggleNavigator();
}

WebInspector.NavigatorOverlayController.prototype = {
    wasShown: function()
    {
        window.setTimeout(this._maybeShowNavigatorOverlay.bind(this), 0);
    },

    _createNavigatorControlButton: function(title, id, listener)
    {
        var button = document.createElement("button");
        button.title = title;
        button.id = id;
        button.addStyleClass("scripts-navigator-control-button");
        button.addEventListener("click", listener, false);
        button.createChild("div", "glyph");
        return button;
    },

    _escDownWhileNavigatorOverlayOpen: function(event)
    {
        this.hideNavigatorOverlay();
    },

    _maybeShowNavigatorOverlay: function()
    {
        if (WebInspector.settings.navigatorHidden.get() && !WebInspector.settings.navigatorWasOnceHidden.get())
            this.showNavigatorOverlay();
    },

    _toggleNavigator: function()
    {
        if (this._navigatorOverlayShown)
            this.hideNavigatorOverlay();
        else if (this._navigatorHidden)
            this.showNavigatorOverlay();
        else
            this._hidePinnedNavigator();
    },

    _hidePinnedNavigator: function()
    {
        this._navigatorHidden = true;
        this._navigatorShowHideButton.removeStyleClass("toggled-on");
        this._navigatorShowHideButton.title = WebInspector.UIString("Show scripts navigator");
        this._editorView.element.addStyleClass("navigator-hidden");
        this._navigatorSidebarResizeWidgetElement.addStyleClass("hidden");

        this._navigatorPinButton.removeStyleClass("hidden");

        this._parentSplitView.hideSidebarElement();
        this._navigatorView.detach();
        this._editorView.focus();
        WebInspector.settings.navigatorHidden.set(true);
    },

    _pinNavigator: function()
    {
        delete this._navigatorHidden;
        this.hideNavigatorOverlay();

        this._navigatorPinButton.addStyleClass("hidden");
        this._navigatorShowHideButton.addStyleClass("toggled-on");
        this._navigatorShowHideButton.title = WebInspector.UIString("Hide scripts navigator");

        this._editorView.element.removeStyleClass("navigator-hidden");
        this._navigatorSidebarResizeWidgetElement.removeStyleClass("hidden");

        this._parentSplitView.showSidebarElement();
        this._navigatorView.show(this._parentSplitView.sidebarElement);
        this._navigatorView.focus();
        WebInspector.settings.navigatorHidden.set(false);
    },

    showNavigatorOverlay: function()
    {
        if (this._navigatorOverlayShown)
            return;

        this._navigatorOverlayShown = true;
        this._sidebarOverlay = new WebInspector.SidebarOverlay(this._navigatorView, "scriptsPanelNavigatorOverlayWidth", Preferences.minScriptsSidebarWidth);
        this._sidebarOverlay.addEventListener(WebInspector.SidebarOverlay.EventTypes.WasShown, this._navigatorOverlayWasShown, this);
        this._sidebarOverlay.addEventListener(WebInspector.SidebarOverlay.EventTypes.WillHide, this._navigatorOverlayWillHide, this);

        var navigatorOverlayResizeWidgetElement = document.createElement("div");
        navigatorOverlayResizeWidgetElement.addStyleClass("scripts-navigator-resizer-widget");
        this._sidebarOverlay.resizerWidgetElement = navigatorOverlayResizeWidgetElement;

        this._sidebarOverlay.show(this._parentSplitView.element);
    },

    hideNavigatorOverlay: function()
    {
        if (!this._navigatorOverlayShown)
            return;

        this._sidebarOverlay.hide();
        this._editorView.focus();
    },

    _navigatorOverlayWasShown: function(event)
    {
        this._navigatorView.element.appendChild(this._navigatorShowHideButton);
        this._navigatorShowHideButton.addStyleClass("toggled-on");
        this._navigatorShowHideButton.title = WebInspector.UIString("Hide navigator");
        this._navigatorView.focus();
        this._panel.registerShortcut(WebInspector.KeyboardShortcut.Keys.Esc.code, this._escDownWhileNavigatorOverlayOpen.bind(this));
    },

    _navigatorOverlayWillHide: function(event)
    {
        delete this._navigatorOverlayShown;
        WebInspector.settings.navigatorWasOnceHidden.set(true);
        this._parentSplitView.element.appendChild(this._navigatorShowHideButton);
        this._navigatorShowHideButton.removeStyleClass("toggled-on");
        this._navigatorShowHideButton.title = WebInspector.UIString("Show navigator");
        this._panel.unregisterShortcut(WebInspector.KeyboardShortcut.Keys.Esc.code);
    }
}
