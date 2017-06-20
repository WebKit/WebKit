/*
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2016 Devin Rousso <dcrousso+webkit@gmail.com>. All rights reserved.
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

WebInspector.SettingsTabContentView = class SettingsTabContentView extends WebInspector.TabContentView
{
    constructor(identifier)
    {
        let tabBarItem = new WebInspector.PinnedTabBarItem("Images/Gear.svg", WebInspector.UIString("Open Settings"));

        super(identifier || "settings", "settings", tabBarItem);

        // Ensures that the Settings tab is displayable from a pinned tab bar item.
        tabBarItem.representedObject = this;

        this._selectedSettingsView = null;
        this._settingsViews = [];
    }

    static tabInfo()
    {
        return {
            image: "Images/Gear.svg",
            title: WebInspector.UIString("Settings"),
        };
    }

    static isEphemeral()
    {
        return true;
    }

    static shouldSaveTab()
    {
        return false;
    }

    // Public

    get type() { return WebInspector.SettingsTabContentView.Type; }

    get selectedSettingsView()
    {
        return this._selectedSettingsView;
    }

    set selectedSettingsView(settingsView)
    {
        if (this._selectedSettingsView === settingsView)
            return;

        if (this._selectedSettingsView)
            this.replaceSubview(this._selectedSettingsView, settingsView);
        else
            this.addSubview(settingsView);

        this._selectedSettingsView = settingsView;
        this._selectedSettingsView.updateLayout();

        let navigationItem = this._navigationBar.findNavigationItem(settingsView.identifier);
        console.assert(navigationItem, "Missing navigation item for settings view.", settingsView)
        if (!navigationItem)
            return;

        this._navigationBar.selectedNavigationItem = navigationItem;
    }

    addSettingsView(settingsView)
    {
        if (this._settingsViews.includes(settingsView)) {
            console.assert(false, "SettingsView already exists.", settingsView);
            return;
        }

        this._settingsViews.push(settingsView);
        this._navigationBar.addNavigationItem(new WebInspector.RadioButtonNavigationItem(settingsView.identifier, settingsView.displayName));

        this._updateNavigationBarVisibility();
    }

    setSettingsViewVisible(settingsView, visible)
    {
        let navigationItem = this._navigationBar.findNavigationItem(settingsView.identifier);
        console.assert(navigationItem, "Missing NavigationItem for identifier: " + settingsView.identifier);
        if (!navigationItem)
            return;

        if (navigationItem.hidden === !visible)
            return;

        navigationItem.hidden = !visible;
        settingsView.element.classList.toggle("hidden", !visible);

        this._updateNavigationBarVisibility();

        if (!this.selectedSettingsView) {
            if (visible)
                this.selectedSettingsView = settingsView;
            return;
        }

        if (this.selectedSettingsView !== settingsView)
            return;

        let index = this._settingsViews.indexOf(settingsView);
        console.assert(index !== -1, "SettingsView not found.", settingsView)
        if (index === -1)
            return;

        let previousIndex = index;
        while (--previousIndex >= 0) {
            let previousNavigationItem = this._navigationBar.navigationItems[previousIndex];
            console.assert(previousNavigationItem);
            if (!previousNavigationItem || previousNavigationItem.hidden)
                continue;

            this.selectedSettingsView = this._settingsViews[previousIndex];
            return;
        }

        let nextIndex = index;
        while (++nextIndex < this._settingsViews.length) {
            let nextNavigationItem = this._navigationBar.navigationItems[nextIndex];
            console.assert(nextNavigationItem);
            if (!nextNavigationItem || nextNavigationItem.hidden)
                continue;

            this.selectedSettingsView = this._settingsViews[nextIndex];
            return;
        }
    }

    // Protected

    initialLayout()
    {
        this._navigationBar = new WebInspector.NavigationBar;
        this._navigationBar.addEventListener(WebInspector.NavigationBar.Event.NavigationItemSelected, this._navigationItemSelected, this);
        this.addSubview(this._navigationBar);

        this._createGeneralSettingsView();

        WebInspector.notifications.addEventListener(WebInspector.Notification.DebugUIEnabledDidChange, this._updateDebugSettingsViewVisibility, this);
        this._updateDebugSettingsViewVisibility();

        this.selectedSettingsView = this._settingsViews[0];
    }

    // Private

    _createGeneralSettingsView()
    {
        let generalSettingsView = new WebInspector.SettingsView("general", WebInspector.UIString("General"));

        const indentValues = [WebInspector.UIString("Tabs"), WebInspector.UIString("Spaces")];

        let indentEditor = generalSettingsView.addGroupWithCustomSetting(WebInspector.UIString("Prefer indent using:"), WebInspector.SettingEditor.Type.Select, {values: indentValues});
        indentEditor.value = indentValues[WebInspector.settings.indentWithTabs.value ? 0 : 1];
        indentEditor.addEventListener(WebInspector.SettingEditor.Event.ValueDidChange, () => {
            WebInspector.settings.indentWithTabs.value = indentEditor.value === indentValues[0];
        });

        const widthLabel = WebInspector.UIString("spaces");
        const widthOptions = {min: 1};

        generalSettingsView.addSetting(WebInspector.UIString("Tab width:"), WebInspector.settings.tabSize, widthLabel, widthOptions);
        generalSettingsView.addSetting(WebInspector.UIString("Indent width:"), WebInspector.settings.indentUnit, widthLabel, widthOptions);

        generalSettingsView.addSetting(WebInspector.UIString("Line wrapping:"), WebInspector.settings.enableLineWrapping, WebInspector.UIString("Wrap lines to editor width"));

        let showGroup = generalSettingsView.addGroup(WebInspector.UIString("Show:"));
        showGroup.addSetting(WebInspector.settings.showWhitespaceCharacters, WebInspector.UIString("Whitespace characters"));
        showGroup.addSetting(WebInspector.settings.showInvalidCharacters, WebInspector.UIString("Invalid characters"));

        generalSettingsView.addSeparator();

        let stylesEditingGroup = generalSettingsView.addGroup(WebInspector.UIString("Styles Editing:"));
        stylesEditingGroup.addSetting(WebInspector.settings.stylesShowInlineWarnings, WebInspector.UIString("Show inline warnings"));
        stylesEditingGroup.addSetting(WebInspector.settings.stylesInsertNewline, WebInspector.UIString("Automatically insert newline"));
        stylesEditingGroup.addSetting(WebInspector.settings.stylesSelectOnFirstClick, WebInspector.UIString("Select text on first click"));

        generalSettingsView.addSeparator();

        generalSettingsView.addSetting(WebInspector.UIString("Network:"), WebInspector.settings.clearNetworkOnNavigate, WebInspector.UIString("Clear when page navigates"));

        generalSettingsView.addSeparator();

        generalSettingsView.addSetting(WebInspector.UIString("Console:"), WebInspector.settings.clearLogOnNavigate, WebInspector.UIString("Clear when page navigates"));

        generalSettingsView.addSeparator();

        generalSettingsView.addSetting(WebInspector.UIString("Debugger:"), WebInspector.settings.showScopeChainOnPause, WebInspector.UIString("Show Scope Chain on pause"));

        generalSettingsView.addSeparator();

        const zoomLevels = [0.6, 0.8, 1, 1.2, 1.4, 1.6, 1.8, 2, 2.2, 2.4];
        const zoomValues = zoomLevels.map((level) => [level, Number.percentageString(level, 0)]);

        let zoomEditor = generalSettingsView.addGroupWithCustomSetting(WebInspector.UIString("Zoom:"), WebInspector.SettingEditor.Type.Select, {values: zoomValues});
        zoomEditor.value = WebInspector.getZoomFactor();
        zoomEditor.addEventListener(WebInspector.SettingEditor.Event.ValueDidChange, () => { WebInspector.setZoomFactor(zoomEditor.value); });
        WebInspector.settings.zoomFactor.addEventListener(WebInspector.Setting.Event.Changed, () => { zoomEditor.value = WebInspector.getZoomFactor().maxDecimals(2); });

        this.addSettingsView(generalSettingsView);
    }

    _createDebugSettingsView()
    {
        if (this._debugSettingsView)
            return;

        // These settings are only ever shown in engineering builds, so the strings are unlocalized.

        this._debugSettingsView = new WebInspector.SettingsView("debug", WebInspector.unlocalizedString("Debug"));

        let protocolMessagesGroup = this._debugSettingsView.addGroup(WebInspector.unlocalizedString("Protocol Logging:"));

        let autoLogProtocolMessagesEditor = protocolMessagesGroup.addSetting(WebInspector.settings.autoLogProtocolMessages, WebInspector.unlocalizedString("Messages"));
        WebInspector.settings.autoLogProtocolMessages.addEventListener(WebInspector.Setting.Event.Changed, () => {
            autoLogProtocolMessagesEditor.value = InspectorBackend.dumpInspectorProtocolMessages;
        });

        protocolMessagesGroup.addSetting(WebInspector.settings.autoLogTimeStats, WebInspector.unlocalizedString("Time Stats"));

        this._debugSettingsView.addSeparator();

        this._debugSettingsView.addSetting(WebInspector.unlocalizedString("Uncaught Exception Reporter:"), WebInspector.settings.enableUncaughtExceptionReporter, WebInspector.unlocalizedString("Enabled"));

        this._debugSettingsView.addSeparator();

        const layoutDirectionValues = [
            [WebInspector.LayoutDirection.System, WebInspector.unlocalizedString("System Default")],
            [WebInspector.LayoutDirection.LTR, WebInspector.unlocalizedString("Left to Right (LTR)")],
            [WebInspector.LayoutDirection.RTL, WebInspector.unlocalizedString("Right to Left (RTL)")],
        ];

        let layoutDirectionEditor = this._debugSettingsView.addGroupWithCustomSetting(WebInspector.unlocalizedString("Layout Direction:"), WebInspector.SettingEditor.Type.Select, {values: layoutDirectionValues});
        layoutDirectionEditor.value = WebInspector.settings.layoutDirection.value;
        layoutDirectionEditor.addEventListener(WebInspector.SettingEditor.Event.ValueDidChange, () => { WebInspector.setLayoutDirection(layoutDirectionEditor.value); });

        if (window.CanvasAgent) {
            this._debugSettingsView.addSeparator();

            this._debugSettingsView.addSetting(WebInspector.UIString("Canvas:"), WebInspector.settings.experimentalShowCanvasContextsInResources, WebInspector.UIString("Show Contexts in Resources Tab"));
        }

        this.addSettingsView(this._debugSettingsView);
    }

    _updateNavigationBarVisibility()
    {
        let visibleItems = 0;
        for (let item of this._navigationBar.navigationItems) {
            if (!item.hidden && ++visibleItems > 1) {
                this._navigationBar.element.classList.remove("invisible");
                return;
            }
        }

        this._navigationBar.element.classList.add("invisible");
    }

    _navigationItemSelected(event)
    {
        let navigationItem = event.target.selectedNavigationItem;
        if (!navigationItem)
            return;

        let settingsView = this._settingsViews.find((view) => view.identifier === navigationItem.identifier);
        console.assert(settingsView, "Missing SettingsView for identifier " + navigationItem.identifier);
        if (!settingsView)
            return;

        this.selectedSettingsView = settingsView;
    }

    _updateDebugSettingsViewVisibility()
    {
        // Only create the Debug view if the debug UI is enabled.
        if (WebInspector.isDebugUIEnabled())
            this._createDebugSettingsView();

        if (!this._debugSettingsView)
            return;

        this.setSettingsViewVisible(this._debugSettingsView, WebInspector.isDebugUIEnabled());

        this.needsLayout();
    }
};

WebInspector.SettingsTabContentView.Type = "settings";
