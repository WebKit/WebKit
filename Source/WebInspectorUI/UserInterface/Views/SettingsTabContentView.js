/*
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2016 Devin Rousso <webkit@devinrousso.com>. All rights reserved.
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

WI.SettingsTabContentView = class SettingsTabContentView extends WI.TabContentView
{
    constructor(identifier)
    {
        let tabBarItem = WI.PinnedTabBarItem.fromTabInfo(WI.SettingsTabContentView.tabInfo());

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
            title: WI.UIString("Settings"),
            isEphemeral: true,
        };
    }

    static shouldSaveTab()
    {
        return false;
    }

    // Public

    get type() { return WI.SettingsTabContentView.Type; }

    get supportsSplitContentBrowser()
    {
        return false;
    }

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
        console.assert(navigationItem, "Missing navigation item for settings view.", settingsView);
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
        this._navigationBar.addNavigationItem(new WI.RadioButtonNavigationItem(settingsView.identifier, settingsView.displayName));

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
        console.assert(index !== -1, "SettingsView not found.", settingsView);
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
        this._navigationBar = new WI.NavigationBar;
        this._navigationBar.addEventListener(WI.NavigationBar.Event.NavigationItemSelected, this._navigationItemSelected, this);
        this.addSubview(this._navigationBar);

        this._createGeneralSettingsView();
        this._createElementsSettingsView();
        this._createSourcesSettingsView();
        this._createConsoleSettingsView();
        this._createExperimentalSettingsView();

        if (WI.isEngineeringBuild)
            this._createEngineeringSettingsView();

        WI.showDebugUISetting.addEventListener(WI.Setting.Event.Changed, this._updateDebugSettingsViewVisibility, this);
        this._updateDebugSettingsViewVisibility();

        this.selectedSettingsView = this._settingsViews[0];
    }

    // Private

    _createGeneralSettingsView()
    {
        let generalSettingsView = new WI.SettingsView("general", WI.UIString("General"));

        const indentValues = [WI.UIString("Tabs"), WI.UIString("Spaces")];
        let indentEditor = generalSettingsView.addGroupWithCustomSetting(WI.UIString("Prefer indent using:"), WI.SettingEditor.Type.Select, {values: indentValues});
        indentEditor.value = indentValues[WI.settings.indentWithTabs.value ? 0 : 1];
        indentEditor.addEventListener(WI.SettingEditor.Event.ValueDidChange, () => {
            WI.settings.indentWithTabs.value = indentEditor.value === indentValues[0];
        });

        function addSpacesSetting(title, setting) {
            let editor = generalSettingsView.addSetting(title, setting, WI.UIString("spaces"), {min: 1});

            function updateLabel() {
                editor.label = setting.value === 1 ? WI.UIString("space") : WI.UIString("spaces");
            }
            setting.addEventListener(WI.Setting.Event.Changed, (event) => {
                updateLabel();
            });
            updateLabel();
        }
        addSpacesSetting(WI.UIString("Tab width:"), WI.settings.tabSize);
        addSpacesSetting(WI.UIString("Indent width:"), WI.settings.indentUnit);

        generalSettingsView.addSetting(WI.UIString("Line wrapping:"), WI.settings.enableLineWrapping, WI.UIString("Wrap lines to editor width"));

        let showGroup = generalSettingsView.addGroup(WI.UIString("Show:"));
        showGroup.addSetting(WI.settings.showWhitespaceCharacters, WI.UIString("Whitespace characters"));
        showGroup.addSetting(WI.settings.showInvisibleCharacters, WI.UIString("Invisible characters"));

        generalSettingsView.addSeparator();

        let searchGroup = generalSettingsView.addGroup(WI.UIString("Search:", "Search: @ Settings", "Settings tab label for search related settings"));
        searchGroup.addSetting(WI.settings.searchCaseSensitive, WI.UIString("Case Sensitive", "Case Sensitive @ Settings", "Settings tab checkbox label for whether searches should be case sensitive."));
        searchGroup.addSetting(WI.settings.searchRegularExpression, WI.UIString("Regular Expression", "Regular Expression @ Settings", "Settings tab checkbox label for whether searches should be treated as regular expressions."));

        generalSettingsView.addSeparator();

        const zoomLevels = [0.6, 0.8, 1, 1.2, 1.4, 1.6, 1.8, 2, 2.2, 2.4];
        const zoomValues = zoomLevels.map((level) => [level, Number.percentageString(level, 0)]);

        let zoomEditor = generalSettingsView.addGroupWithCustomSetting(WI.UIString("Zoom:"), WI.SettingEditor.Type.Select, {values: zoomValues});
        zoomEditor.value = WI.getZoomFactor();
        zoomEditor.addEventListener(WI.SettingEditor.Event.ValueDidChange, () => { WI.setZoomFactor(zoomEditor.value); });
        WI.settings.zoomFactor.addEventListener(WI.Setting.Event.Changed, () => { zoomEditor.value = WI.getZoomFactor().maxDecimals(2); });

        this.addSettingsView(generalSettingsView);
    }

    _createElementsSettingsView()
    {
        if (!InspectorBackend.domains.DOM)
            return;

        let elementsSettingsView = new WI.SettingsView("elements", WI.UIString("Elements"));

        if (InspectorBackend.domains.DOM.setInspectModeEnabled && InspectorBackend.domains.DOM.setInspectModeEnabled.supports("showRulers")) {
            elementsSettingsView.addSetting(WI.UIString("Element Selection:"), WI.settings.showRulersDuringElementSelection, WI.UIString("Show page rulers and node border lines"));

            elementsSettingsView.addSeparator();
        }

        elementsSettingsView.addSetting(WI.UIString("CSS Changes:"), WI.settings.cssChangesPerNode, WI.UIString("Show only for selected node"));

        this.addSettingsView(elementsSettingsView);
    }

    _createSourcesSettingsView()
    {
        let sourcesSettingsView = new WI.SettingsView("sources", WI.UIString("Sources"));

        sourcesSettingsView.addSetting(WI.UIString("Debugger:"), WI.settings.showScopeChainOnPause, WI.UIString("Show Scope Chain on pause"));

        sourcesSettingsView.addSeparator();

        sourcesSettingsView.addSetting(WI.UIString("Source Maps:"), WI.settings.sourceMapsEnabled, WI.UIString("Enable source maps"));

        this.addSettingsView(sourcesSettingsView);
    }

    _createConsoleSettingsView()
    {
        let consoleSettingsView = new WI.SettingsView("console", WI.UIString("Console"));

        // COMPATIBILITY (iOS 12.2): Runtime.setSavedResultAlias did not exist.
        if (InspectorBackend.domains.Runtime.setSavedResultAlias) {
            let consoleSavedResultAliasEditor = consoleSettingsView.addGroupWithCustomEditor(WI.UIString("Saved Result Alias:"));

            let consoleSavedResultAliasInput = consoleSavedResultAliasEditor.appendChild(document.createElement("input"));
            consoleSavedResultAliasInput.type = "text";
            consoleSavedResultAliasInput.value = WI.settings.consoleSavedResultAlias.value;
            consoleSavedResultAliasInput.placeholder = WI.unlocalizedString("$");
            consoleSavedResultAliasInput.addEventListener("keydown", (event) => {
                if (!/[a-zA-Z0-9_$]/.test(event.key) || (consoleSavedResultAliasInput.selectionStart === 0 && /[0-9]/.test(event.key))) {
                    event.preventDefault();
                    InspectorFrontendHost.beep();
                }
            });
            consoleSavedResultAliasInput.addEventListener("input", (event) => {
                let savedResultAlias = consoleSavedResultAliasInput.value;
                if (savedResultAlias === "$")
                    savedResultAlias = "";
                WI.settings.consoleSavedResultAlias.value = savedResultAlias;
            });

            consoleSettingsView.addSeparator();
        }

        consoleSettingsView.addSetting(WI.UIString("Traces:"), WI.settings.consoleAutoExpandTrace, WI.UIString("Auto-expand"));

        if (WI.ConsoleManager.supportsLogChannels()) {
            consoleSettingsView.addSeparator();

            const logLevels = [
                [WI.LoggingChannel.Level.Off, WI.UIString("Off")],
                [WI.LoggingChannel.Level.Basic, WI.UIString("Basic")],
                [WI.LoggingChannel.Level.Verbose, WI.UIString("Verbose")],
            ];
            const editorLabels = {
                media: WI.UIString("Media Logging:"),
                mediasource: WI.UIString("MSE Logging:"),
                webrtc: WI.UIString("WebRTC Logging:"),
            };

            let channels = WI.consoleManager.customLoggingChannels;
            for (let channel of channels) {
                let logEditor = consoleSettingsView.addGroupWithCustomSetting(editorLabels[channel.source], WI.SettingEditor.Type.Select, {values: logLevels});
                logEditor.value = channel.level;
                logEditor.addEventListener(WI.SettingEditor.Event.ValueDidChange, () => {
                    for (let target of WI.targets)
                        target.ConsoleAgent.setLoggingChannelLevel(channel.source, logEditor.value);
                });
            }
        }

        this.addSettingsView(consoleSettingsView);
    }

    _createExperimentalSettingsView()
    {
        let experimentalSettingsView = new WI.SettingsView("experimental", WI.UIString("Experimental"));

        let initialValues = new Map;

        experimentalSettingsView.addSetting(WI.UIString("Sources:"), WI.settings.experimentalEnableSourcesTab, WI.UIString("Enable Sources Tab"));
        experimentalSettingsView.addSeparator();

        if (window.LayerTreeAgent) {
            experimentalSettingsView.addSetting(WI.UIString("Layers:"), WI.settings.experimentalEnableLayersTab, WI.UIString("Enable Layers Tab"));
            experimentalSettingsView.addSeparator();
        }

        experimentalSettingsView.addSetting(WI.UIString("User Interface:"), WI.settings.experimentalEnableNewTabBar, WI.UIString("Enable New Tab Bar"));
        experimentalSettingsView.addSeparator();

        if (InspectorBackend.domains.CSS) {
            let stylesGroup = experimentalSettingsView.addGroup(WI.UIString("Styles:"));
            stylesGroup.addSetting(WI.settings.experimentalEnableStylesIcons, WI.UIString("Show Icons"));
            stylesGroup.addSetting(WI.settings.experimentalEnableStylesJumpToEffective, WI.UIString("Show Jump to Effective Property Button"));
            experimentalSettingsView.addSeparator();
        }

        let reloadInspectorButton = document.createElement("button");
        reloadInspectorButton.textContent = WI.UIString("Reload Web Inspector");
        reloadInspectorButton.addEventListener("click", (event) => {
            // Force a copy so that WI.Setting sees it as a new value.
            let newTabs = WI._openTabsSetting.value.slice();
            if (!initialValues.get(WI.settings.experimentalEnableSourcesTab) && WI.settings.experimentalEnableSourcesTab.value)
                newTabs.push(WI.SourcesTabContentView.Type);
            if (!initialValues.get(WI.settings.experimentalEnableLayersTab) && window.LayerTreeAgent && WI.settings.experimentalEnableLayersTab.value)
                newTabs.push(WI.LayersTabContentView.Type);
            WI._openTabsSetting.value = newTabs;

            InspectorFrontendHost.reopen();
        });

        let reloadInspectorContainerElement = experimentalSettingsView.addCenteredContainer(reloadInspectorButton, WI.UIString("for changes to take effect"));
        reloadInspectorContainerElement.classList.add("hidden");

        function listenForChange(setting) {
            initialValues.set(setting, setting.value);
            setting.addEventListener(WI.Setting.Event.Changed, () => {
                reloadInspectorContainerElement.classList.toggle("hidden", Array.from(initialValues).every(([setting, initialValue]) => setting.value === initialValue));
            });
        }

        listenForChange(WI.settings.experimentalEnableSourcesTab);
        listenForChange(WI.settings.experimentalEnableLayersTab);
        listenForChange(WI.settings.experimentalEnableNewTabBar);

        if (InspectorBackend.domains.CSS) {
            listenForChange(WI.settings.experimentalEnableStylesIcons);
            listenForChange(WI.settings.experimentalEnableStylesJumpToEffective);
        }

        this.addSettingsView(experimentalSettingsView);
    }

    _createEngineeringSettingsView()
    {
        // These settings are only ever shown in engineering builds, so the strings are unlocalized.

        let engineeringSettingsView = new WI.SettingsView("engineering", WI.unlocalizedString("Engineering"));

        let debuggingGroup = engineeringSettingsView.addGroup(WI.unlocalizedString("Debugging:"));
        debuggingGroup.addSetting(WI.settings.engineeringShowInternalScripts, WI.unlocalizedString("Show WebKit-internal scripts"));
        debuggingGroup.addSetting(WI.settings.engineeringPauseForInternalScripts, WI.unlocalizedString("Pause in WebKit-internal scripts"));

        engineeringSettingsView.addSeparator();

        let heapSnapshotGroup = engineeringSettingsView.addGroup(WI.unlocalizedString("Heap Snapshot:"));
        heapSnapshotGroup.addSetting(WI.settings.engineeringShowInternalObjectsInHeapSnapshot, WI.unlocalizedString("Show Internal Objects"));
        heapSnapshotGroup.addSetting(WI.settings.engineeringShowPrivateSymbolsInHeapSnapshot, WI.unlocalizedString("Show Private Symbols"));

        this.addSettingsView(engineeringSettingsView);
    }

    _createDebugSettingsView()
    {
        if (this._debugSettingsView)
            return;

        // These settings are only ever shown in engineering builds, so the strings are unlocalized.

        this._debugSettingsView = new WI.SettingsView("debug", WI.unlocalizedString("Debug"));

        let protocolMessagesGroup = this._debugSettingsView.addGroup(WI.unlocalizedString("Protocol Logging:"));

        let protocolAutoLogMessagesEditor = protocolMessagesGroup.addSetting(WI.settings.protocolAutoLogMessages, WI.unlocalizedString("Messages"));
        WI.settings.protocolAutoLogMessages.addEventListener(WI.Setting.Event.Changed, () => {
            protocolAutoLogMessagesEditor.value = InspectorBackend.dumpInspectorProtocolMessages;
        });

        protocolMessagesGroup.addSetting(WI.settings.protocolAutoLogTimeStats, WI.unlocalizedString("Time Stats"));
        protocolMessagesGroup.addSetting(WI.settings.protocolLogAsText, WI.unlocalizedString("Log as Text"));

        this._debugSettingsView.addSeparator();

        this._debugSettingsView.addSetting(WI.unlocalizedString("Debugging:"), WI.settings.debugShowConsoleEvaluations, WI.unlocalizedString("Show Console Evaluations"));

        this._debugSettingsView.addSeparator();

        this._debugSettingsView.addSetting(WI.unlocalizedString("Layout Flashing:"), WI.settings.debugEnableLayoutFlashing, WI.unlocalizedString("Draw borders when a view performs a layout"));

        this._debugSettingsView.addSeparator();

        this._debugSettingsView.addSetting(WI.unlocalizedString("Styles:"), WI.settings.debugEnableStyleEditingDebugMode, WI.unlocalizedString("Enable style editing debug mode"));

        this._debugSettingsView.addSeparator();


        this._debugSettingsView.addSetting(WI.unlocalizedString("Uncaught Exception Reporter:"), WI.settings.debugEnableUncaughtExceptionReporter, WI.unlocalizedString("Enabled"));

        this._debugSettingsView.addSeparator();

        const layoutDirectionValues = [
            [WI.LayoutDirection.System, WI.unlocalizedString("System Default")],
            [WI.LayoutDirection.LTR, WI.unlocalizedString("Left to Right (LTR)")],
            [WI.LayoutDirection.RTL, WI.unlocalizedString("Right to Left (RTL)")],
        ];

        let layoutDirectionEditor = this._debugSettingsView.addGroupWithCustomSetting(WI.unlocalizedString("Layout Direction:"), WI.SettingEditor.Type.Select, {values: layoutDirectionValues});
        layoutDirectionEditor.value = WI.settings.debugLayoutDirection.value;
        layoutDirectionEditor.addEventListener(WI.SettingEditor.Event.ValueDidChange, () => { WI.setLayoutDirection(layoutDirectionEditor.value); });

        this._debugSettingsView.addSeparator();

        let resetInspectorButton = document.createElement("button");
        resetInspectorButton.textContent = WI.unlocalizedString("Reset Web Inspector");
        resetInspectorButton.addEventListener("click", (event) => {
            WI.reset();
        });
        this._debugSettingsView.addCenteredContainer(resetInspectorButton);

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
        if (WI.isDebugUIEnabled())
            this._createDebugSettingsView();

        if (!this._debugSettingsView)
            return;

        this.setSettingsViewVisible(this._debugSettingsView, WI.isDebugUIEnabled());

        this.needsLayout();
    }
};

WI.SettingsTabContentView.Type = "settings";
