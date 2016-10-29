/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

WebInspector.NewTabContentView = class NewTabContentView extends WebInspector.TabContentView
{
    constructor(identifier)
    {
        let {image, title} = WebInspector.NewTabContentView.tabInfo();
        let tabBarItem = new WebInspector.GeneralTabBarItem(image, title);
        tabBarItem.isDefaultTab = true;

        super(identifier || "new-tab", "new-tab", tabBarItem);

        WebInspector.notifications.addEventListener(WebInspector.Notification.TabTypesChanged, this._updateShownTabs.bind(this));

        this._tabElementsByTabClass = new Map;
        this._updateShownTabs();
    }

    static tabInfo()
    {
        return {
            image: "Images/NewTab.svg",
            title: WebInspector.UIString("New Tab"),
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

    get type()
    {
        return WebInspector.NewTabContentView.Type;
    }

    shown()
    {
        WebInspector.tabBrowser.tabBar.addEventListener(WebInspector.TabBar.Event.TabBarItemAdded, this._updateTabItems, this);
        WebInspector.tabBrowser.tabBar.addEventListener(WebInspector.TabBar.Event.TabBarItemRemoved, this._updateTabItems, this);

        this._updateTabItems();
    }

    hidden()
    {
        WebInspector.tabBrowser.tabBar.removeEventListener(null, null, this);
    }

    get supportsSplitContentBrowser()
    {
        // Showing the split console is problematic because some new tabs will cause it to
        // disappear and not reappear, but others won't. Just prevent it from ever showing.
        return false;
    }

    layout()
    {
        this._tabElementsByTabClass.clear();
        this.element.removeChildren();

        for (let tabClass of this._shownTabClasses) {
            let tabItemElement = document.createElement("div");
            tabItemElement.classList.add("tab-item");
            tabItemElement.addEventListener("click", this._createNewTabWithType.bind(this, tabClass.Type));
            tabItemElement[WebInspector.NewTabContentView.TypeSymbol] = tabClass.Type;

            let boxElement = tabItemElement.appendChild(document.createElement("div"));
            boxElement.classList.add("box");

            let info = tabClass.tabInfo();
            let imageElement = boxElement.appendChild(document.createElement("img"));
            imageElement.src = info.image;

            let labelElement = tabItemElement.appendChild(document.createElement("label"));
            labelElement.textContent = info.title;

            this.element.appendChild(tabItemElement);
            this._tabElementsByTabClass.set(tabClass, tabItemElement);
        }

        this._updateTabItems();
    }

    // Private

    _createNewTabWithType(tabType, event)
    {
        if (!WebInspector.isNewTabWithTypeAllowed(tabType))
            return;

        const canCreateAdditionalTabs = this._allowableTabTypes().length > 1;
        const options = {
            referencedView: this,
            shouldReplaceTab: !canCreateAdditionalTabs || !WebInspector.modifierKeys.metaKey,
            shouldShowNewTab: !WebInspector.modifierKeys.metaKey
        };
        WebInspector.createNewTabWithType(tabType, options);
    }

    _updateShownTabs()
    {
        let allTabClasses = Array.from(WebInspector.knownTabClasses());
        let allowedTabClasses = allTabClasses.filter((tabClass) => tabClass.isTabAllowed() && !tabClass.isEphemeral());
        allowedTabClasses.sort((a, b) => a.tabInfo().title.localeCompare(b.tabInfo().title));

        if (Array.shallowEqual(this._shownTabClasses, allowedTabClasses))
            return;

        this._shownTabClasses = allowedTabClasses;
        this.needsLayout();
    }

    _allowableTabTypes()
    {
        let tabTypes = this._shownTabClasses.map((tabClass) => tabClass.Type);
        return tabTypes.filter((type) => WebInspector.isNewTabWithTypeAllowed(type));
    }

    _updateTabItems()
    {
        for (let [tabClass, tabItemElement] of this._tabElementsByTabClass.entries()) {
            let allowed = WebInspector.isNewTabWithTypeAllowed(tabClass.Type);
            tabItemElement.classList.toggle(WebInspector.NewTabContentView.DisabledStyleClassName, !allowed);
        }
    }
};

WebInspector.NewTabContentView.Type = "new-tab";
WebInspector.NewTabContentView.TypeSymbol = Symbol("type");

WebInspector.NewTabContentView.TabItemStyleClassName = "tab-item";
WebInspector.NewTabContentView.DisabledStyleClassName = "disabled";
