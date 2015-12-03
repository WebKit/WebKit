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
        var tabBarItem = new WebInspector.TabBarItem("Images/NewTab.svg", WebInspector.UIString("New Tab"));
        tabBarItem.isDefaultTab = true;

        super(identifier || "new-tab", "new-tab", tabBarItem);

        var allowedNewTabs = [
            {image: "Images/Console.svg", title: WebInspector.UIString("Console"), type: WebInspector.ConsoleTabContentView.Type},
            {image: "Images/Debugger.svg", title: WebInspector.UIString("Debugger"), type: WebInspector.DebuggerTabContentView.Type},
            {image: "Images/Elements.svg", title: WebInspector.UIString("Elements"), type: WebInspector.ElementsTabContentView.Type},
            {image: "Images/Network.svg", title: WebInspector.UIString("Network"), type: WebInspector.NetworkTabContentView.Type},
            {image: "Images/Resources.svg", title: WebInspector.UIString("Resources"), type: WebInspector.ResourcesTabContentView.Type},
            {image: "Images/Storage.svg", title: WebInspector.UIString("Storage"), type: WebInspector.StorageTabContentView.Type},
            {image: "Images/Timeline.svg", title: WebInspector.UIString("Timelines"), type: WebInspector.TimelineTabContentView.Type}
        ];

        allowedNewTabs.sort(function(a, b) { return a.title.localeCompare(b.title); });

        for (var info of allowedNewTabs) {
            if (!WebInspector.isTabTypeAllowed(info.type))
                continue;

            var tabItemElement = document.createElement("div");
            tabItemElement.classList.add(WebInspector.NewTabContentView.TabItemStyleClassName);
            tabItemElement.addEventListener("click", this._createNewTabWithType.bind(this, info.type));
            tabItemElement[WebInspector.NewTabContentView.TypeSymbol] = info.type;

            var boxElement = tabItemElement.appendChild(document.createElement("div"));
            boxElement.classList.add("box");

            var imageElement = boxElement.appendChild(document.createElement("img"));
            imageElement.src = info.image;

            var labelElement = tabItemElement.appendChild(document.createElement("label"));
            labelElement.textContent = info.title;

            this.element.appendChild(tabItemElement);
        }
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

    get tabItemElements()
    {
        return Array.from(this.element.querySelectorAll("." + WebInspector.NewTabContentView.TabItemStyleClassName));
    }

    // Private

    _createNewTabWithType(tabType, event)
    {
        if (!WebInspector.isNewTabWithTypeAllowed(tabType))
            return;

        var canCreateAdditionalTabs = this._allowableTabTypes().length > 1;
        var options = {
            referencedView: this,
            shouldReplaceTab: !canCreateAdditionalTabs || !WebInspector.modifierKeys.metaKey,
            shouldShowNewTab: !WebInspector.modifierKeys.metaKey
        }
        WebInspector.createNewTabWithType(tabType, options);
    }

    _allowableTabTypes()
    {
        var tabItemElements = this.tabItemElements;
        var tabTypes = tabItemElements.map(function(tabItemElement) { return tabItemElement[WebInspector.NewTabContentView.TypeSymbol]; });
        return tabTypes.filter(function(type) { return WebInspector.isNewTabWithTypeAllowed(type); });
    }

    _updateTabItems()
    {
        var tabItemElements = this.tabItemElements;
        for (var tabItemElement of tabItemElements) {
            var type = tabItemElement[WebInspector.NewTabContentView.TypeSymbol];
            var allowed = WebInspector.isNewTabWithTypeAllowed(type);
            tabItemElement.classList.toggle(WebInspector.NewTabContentView.DisabledStyleClassName, !allowed);
        }
    }
};

WebInspector.NewTabContentView.Type = "new-tab";
WebInspector.NewTabContentView.TypeSymbol = Symbol("type");

WebInspector.NewTabContentView.TabItemStyleClassName = "tab-item";
WebInspector.NewTabContentView.DisabledStyleClassName = "disabled";
