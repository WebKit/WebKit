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
        this._navigationBar.element.classList.add("hidden");
        this._navigationBar.addEventListener(WebInspector.NavigationBar.Event.NavigationItemSelected, this._navigationItemSelected, this);

        this._selectedSettingsView = null;
        this._settingsViewIdentifierMap = new Map;

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

    set selectedSettingsView(page)
    {
        if (this._selectedSettingsView === page)
            return;

        if (this._selectedSettingsView)
            this.replaceSubview(this._selectedSettingsView, page);
        else
            this.addSubview(page);

        this._selectedSettingsView = page;
        this._selectedSettingsView.updateLayout();

        this._navigationBar.selectedNavigationItem = page.identifier;
    }

    addSettingsView(settingsView)
    {
        let identifier = settingsView.identifier;
        console.assert(!this._settingsViewIdentifierMap.has(identifier), "SettingsView already exists.", settingsView);
        if (this._settingsViewIdentifierMap.has(identifier))
            return;

        this._settingsViewIdentifierMap.set(identifier, settingsView);

        this._navigationBar.addNavigationItem(new WebInspector.RadioButtonNavigationItem(identifier, settingsView.displayName));

        if (this._settingsViewIdentifierMap.size > 1)
            this._navigationBar.element.classList.remove("hidden");
    }

    // Private

    _navigationItemSelected(event)
    {
        let navigationItem = event.target.selectedNavigationItem;
        if (!navigationItem)
            return;

        let settingsView = this._settingsViewIdentifierMap.get(navigationItem.identifier);
        console.assert(settingsView, "Missing SettingsView for identifier " + navigationItem.identifier);
        if (!settingsView)
            return;

        this.selectedSettingsView = settingsView;
    }
};

WebInspector.SettingsTabContentView.Type = "settings";
