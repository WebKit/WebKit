/*
 * Copyright (C) 2013-2015 Apple Inc. All rights reserved.
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

WebInspector.DefaultDashboardView = class DefaultDashboardView extends WebInspector.DashboardView
{
    constructor(representedObject)
    {
        super(representedObject, "default");

        representedObject.addEventListener(WebInspector.DefaultDashboard.Event.DataDidChange, window.requestAnimationFrame.bind(null, this._updateDisplay.bind(this)));

        this._items = {
            resourcesCount: {
                tooltip: WebInspector.UIString("Total number of resources, click to show the Resources tab"),
                handler: this._resourcesItemWasClicked
            },
            resourcesSize: {
                tooltip: WebInspector.UIString("Total size of all resources, click to show the Network Requests timeline"),
                handler: this._networkItemWasClicked
            },
            time: {
                tooltip: WebInspector.UIString("Time until the load event fired, click to show the Network Requests timeline"),
                handler: this._timelineItemWasClicked
            },
            logs: {
                tooltip: WebInspector.UIString("Console logs, click to show the Console tab"),
                handler: this._consoleItemWasClicked.bind(this, WebInspector.LogContentView.Scopes.Logs)
            },
            errors: {
                tooltip: WebInspector.UIString("Console errors, click to show the Console tab"),
                handler: this._consoleItemWasClicked.bind(this, WebInspector.LogContentView.Scopes.Errors)
            },
            issues: {
                tooltip: WebInspector.UIString("Console warnings, click to show the Console tab"),
                handler: this._consoleItemWasClicked.bind(this, WebInspector.LogContentView.Scopes.Warnings)
            }
        };

        for (var name in this._items)
            this._appendElementForNamedItem(name);
    }
    
    // Private

    _updateDisplay()
    {
        var dashboard = this.representedObject;

        for (var category of ["logs", "issues", "errors"])
            this._setConsoleItemValue(category, dashboard[category]);

        var timeItem = this._items.time;
        timeItem.text = dashboard.time ? Number.secondsToString(dashboard.time) : emDash;
        this._setItemEnabled(timeItem, dashboard.time > 0);

        var countItem = this._items.resourcesCount;
        countItem.text = this._formatPossibleLargeNumber(dashboard.resourcesCount);
        this._setItemEnabled(countItem, dashboard.resourcesCount > 0);

        var sizeItem = this._items.resourcesSize;
        sizeItem.text = dashboard.resourcesSize ? Number.bytesToString(dashboard.resourcesSize, false) : emDash;
        this._setItemEnabled(sizeItem, dashboard.resourcesSize > 0);
    }

    _formatPossibleLargeNumber(number)
    {
        return number > 999 ? WebInspector.UIString("999+") : number;
    }

    _appendElementForNamedItem(name)
    {
        var item = this._items[name];

        item.container = this._element.appendChild(document.createElement("div"));
        item.container.className = "item " + name;
        item.container.title = item.tooltip;

        item.container.appendChild(document.createElement("img"));

        item.outlet = item.container.appendChild(document.createElement("div"));

        Object.defineProperty(item, "text", {
            set: function(newText)
            {
                if (newText === item.outlet.textContent)
                    return;
                item.outlet.textContent = newText;
            }
        });

        item.container.addEventListener("click", function(event) {
            this._itemWasClicked(name);
        }.bind(this));
    }

    _itemWasClicked(name)
    {
        var item = this._items[name];
        if (!item.container.classList.contains(WebInspector.DefaultDashboardView.EnabledItemStyleClassName))
            return;

        if (item.handler)
            item.handler.call(this);
    }

    _resourcesItemWasClicked()
    {
        WebInspector.showResourcesTab();
    }

    _networkItemWasClicked()
    {
        WebInspector.showNetworkTab();
    }

    _timelineItemWasClicked()
    {
        WebInspector.showTimelineTab();
    }

    _consoleItemWasClicked(scope)
    {
        WebInspector.showConsoleTab(scope);
    }

    _setConsoleItemValue(itemName, newValue)
    {
        var iVarName = "_" + itemName;
        var previousValue = this[iVarName];
        this[iVarName] = newValue;

        var item = this._items[itemName];
        item.text = this._formatPossibleLargeNumber(newValue);
        this._setItemEnabled(item, newValue > 0);

        if (newValue <= previousValue)
            return;

        var container = item.container;

        function animationEnded(event)
        {
            if (event.target === container) {
                container.classList.remove("pulsing");
                container.removeEventListener("animationend", animationEnded);
            }
        }

        // We need to force a style invalidation in the case where we already
        // were animating this item after we've removed the pulsing CSS class.
        if (container.classList.contains("pulsing")) {
            container.classList.remove("pulsing");
            container.recalculateStyles();
        } else
            container.addEventListener("animationend", animationEnded);

        container.classList.add("pulsing");
    }

    _setItemEnabled(item, enabled)
    {
        if (enabled)
            item.container.classList.add(WebInspector.DefaultDashboardView.EnabledItemStyleClassName);
        else
            item.container.classList.remove(WebInspector.DefaultDashboardView.EnabledItemStyleClassName);
    }
};

WebInspector.DefaultDashboardView.EnabledItemStyleClassName = "enabled";
