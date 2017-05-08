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

        let boundNeedsLayout = this.needsLayout.bind(this, WebInspector.View.LayoutReason.Dirty);
        WebInspector.notifications.addEventListener(WebInspector.Notification.DebugUIEnabledDidChange, boundNeedsLayout);
        WebInspector.settings.zoomFactor.addEventListener(WebInspector.Setting.Event.Changed, boundNeedsLayout);

        this._navigationBar = new WebInspector.NavigationBar;
        this._navigationBar.addEventListener(WebInspector.NavigationBar.Event.NavigationItemSelected, this._navigationItemSelected, this);

        this._selectedSettingsView = null;
        this._settingsViews = [];

        this.addSubview(this._navigationBar);

        let generalSettingsView = new WebInspector.GeneralSettingsView;
        this.addSettingsView(generalSettingsView);

        this.selectedSettingsView = generalSettingsView;
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

    // Private

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
};

WebInspector.SettingsTabContentView.Type = "settings";
