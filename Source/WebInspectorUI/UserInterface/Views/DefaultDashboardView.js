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

WI.DefaultDashboardView = class DefaultDashboardView extends WI.DashboardView
{
    constructor(representedObject)
    {
        super(representedObject, "default");

        representedObject.addEventListener(WI.DefaultDashboard.Event.DataDidChange, () => { this._updateDisplaySoon(); });
        this._scheduledUpdateIdentifier = undefined;

        this._items = {
            resourcesCount: {
                tooltip: WI.UIString("Show page resources"),
                handler: this._resourcesItemWasClicked
            },
            resourcesSize: {
                tooltip: WI.UIString("Show network information"),
                handler: this._networkItemWasClicked
            },
            time: {
                tooltip: WI.UIString("Show page load timing"),
                handler: this._timelineItemWasClicked
            },
            logs: {
                tooltip: WI.UIString("Show messages logged to the Console"),
                handler: this._consoleItemWasClicked.bind(this, WI.LogContentView.Scopes.Logs)
            },
            errors: {
                tooltip: WI.UIString("Show errors logged to the Console"),
                handler: this._consoleItemWasClicked.bind(this, WI.LogContentView.Scopes.Errors)
            },
            issues: {
                tooltip: WI.UIString("Show warnings logged to the Console"),
                handler: this._consoleItemWasClicked.bind(this, WI.LogContentView.Scopes.Warnings)
            }
        };

        for (var name in this._items)
            this._appendElementForNamedItem(name);
    }

    // Private

    _updateDisplaySoon()
    {
        if (this._scheduledUpdateIdentifier)
            return;

        this._scheduledUpdateIdentifier = requestAnimationFrame(this._updateDisplay.bind(this));
    }

    _updateDisplay()
    {
        this._scheduledUpdateIdentifier = undefined;

        var dashboard = this.representedObject;

        for (var category of ["logs", "issues", "errors"])
            this._setConsoleItemValue(category, dashboard[category]);

        var timeItem = this._items.time;
        timeItem.text = dashboard.time ? Number.secondsToString(dashboard.time) : emDash;
        this._setItemEnabled(timeItem, dashboard.time > 0);

        var countItem = this._items.resourcesCount;
        countItem.text = Number.abbreviate(dashboard.resourcesCount);
        this._setItemEnabled(countItem, dashboard.resourcesCount > 0);

        var sizeItem = this._items.resourcesSize;
        sizeItem.text = dashboard.resourcesSize ? Number.bytesToString(dashboard.resourcesSize, false) : emDash;
        this._setItemEnabled(sizeItem, dashboard.resourcesSize > 0);
    }

    _appendElementForNamedItem(name)
    {
        var item = this._items[name];

        item.container = this._element.appendChild(document.createElement("div"));
        item.container.className = "item " + name;

        item.container.appendChild(document.createElement("img"));

        item.outlet = item.container.appendChild(document.createElement("div"));

        Object.defineProperty(item, "text", {
            set: function(newText)
            {
                newText = newText.toString();
                if (newText === item.outlet.textContent)
                    return;
                item.outlet.textContent = newText;
            }
        });

        item.container.addEventListener("click", (event) => {
            this._itemWasClicked(name);
        });
    }

    _itemWasClicked(name)
    {
        var item = this._items[name];
        if (!item.container.classList.contains(WI.DefaultDashboardView.EnabledItemStyleClassName))
            return;

        if (item.handler)
            item.handler.call(this);
    }

    _resourcesItemWasClicked()
    {
        if (WI.settings.experimentalEnableSourcesTab.value)
            WI.showSourcesTab();
        else
            WI.showResourcesTab();
    }

    _networkItemWasClicked()
    {
        WI.showNetworkTab();
    }

    _timelineItemWasClicked()
    {
        WI.showTimelineTab();
    }

    _consoleItemWasClicked(scope)
    {
        WI.showConsoleTab(scope);
    }

    _setConsoleItemValue(itemName, newValue)
    {
        var iVarName = "_" + itemName;
        var previousValue = this[iVarName];
        this[iVarName] = newValue;

        var item = this._items[itemName];
        item.text = Number.abbreviate(newValue);
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
        if (enabled) {
            item.container.title = item.tooltip;
            item.container.classList.add(WI.DefaultDashboardView.EnabledItemStyleClassName);
        } else {
            item.container.title = "";
            item.container.classList.remove(WI.DefaultDashboardView.EnabledItemStyleClassName);
        }
    }
};

WI.DefaultDashboardView.EnabledItemStyleClassName = "enabled";
