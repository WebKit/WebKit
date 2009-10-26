/*
 * Copyright (C) 2007, 2008 Apple Inc.  All rights reserved.
 * Copyright (C) 2008, 2009 Anthony Ricaud <rik@webkit.org>
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

WebInspector.AbstractTimelinePanel = function()
{
    WebInspector.Panel.call(this);
}

WebInspector.AbstractTimelinePanel.prototype = {
    get categories()
    {
        // Should be implemented by the concrete subclasses.
        return {};
    },

    showCategory: function(category)
    {
        // Should be implemented by the concrete subclasses.
    },

    hideCategory: function(category)
    {
        // Should be implemented by the concrete subclasses.
    },

    createInterface: function()
    {
        this._createFilterPanel();

        this.containerElement = document.createElement("div");
        this.containerElement.id = "resources-container";
        this.containerElement.addEventListener("scroll", this._updateDividersLabelBarPosition.bind(this), false);
        this.element.appendChild(this.containerElement);

        this.createSidebar(this.containerElement, this.element);
        this.sidebarElement.id = "resources-sidebar";
        this.populateSidebar();

        this._createGraph();
    },

    _createFilterPanel: function()
    {
        this.filterBarElement = document.createElement("div");
        this.filterBarElement.id = "resources-filter";
        this.filterBarElement.className = "scope-bar";
        this.element.appendChild(this.filterBarElement);

        function createFilterElement(category)
        {
            if (category === "all")
                var label = WebInspector.UIString("All");
            else if (this.categories[category])
                var label = this.categories[category].title;

            var categoryElement = document.createElement("li");
            categoryElement.category = category;
            categoryElement.addStyleClass(category);
            categoryElement.appendChild(document.createTextNode(label));
            categoryElement.addEventListener("click", this._updateFilter.bind(this), false);
            this.filterBarElement.appendChild(categoryElement);

            return categoryElement;
        }

        this.filterAllElement = createFilterElement.call(this, "all");

        // Add a divider
        var dividerElement = document.createElement("div");
        dividerElement.addStyleClass("divider");
        this.filterBarElement.appendChild(dividerElement);

        for (var category in this.categories)
            createFilterElement.call(this, category);
    },

    filter: function(target, selectMultiple)
    {
        function unselectAll()
        {
            for (var i = 0; i < this.filterBarElement.childNodes.length; ++i) {
                var child = this.filterBarElement.childNodes[i];
                if (!child.category)
                    continue;

                child.removeStyleClass("selected");
                this.hideCategory(child.category);
            }
        }

        if (target === this.filterAllElement) {
            if (target.hasStyleClass("selected")) {
                // We can't unselect All, so we break early here
                return;
            }

            // If All wasn't selected, and now is, unselect everything else.
            unselectAll.call(this);
        } else {
            // Something other than All is being selected, so we want to unselect All.
            if (this.filterAllElement.hasStyleClass("selected")) {
                this.filterAllElement.removeStyleClass("selected");
                this.hideCategory("all");
            }
        }

        if (!selectMultiple) {
            // If multiple selection is off, we want to unselect everything else
            // and just select ourselves.
            unselectAll.call(this);

            target.addStyleClass("selected");
            this.showCategory(target.category);
            return;
        }

        if (target.hasStyleClass("selected")) {
            // If selectMultiple is turned on, and we were selected, we just
            // want to unselect ourselves.
            target.removeStyleClass("selected");
            this.hideCategory(target.category);
        } else {
            // If selectMultiple is turned on, and we weren't selected, we just
            // want to select ourselves.
            target.addStyleClass("selected");
            this.showCategory(target.category);
        }
    },

    _updateFilter: function(e)
    {
        var isMac = InspectorController.platform().indexOf("mac-") === 0;
        var selectMultiple = false;
        if (isMac && e.metaKey && !e.ctrlKey && !e.altKey && !e.shiftKey)
            selectMultiple = true;
        if (!isMac && e.ctrlKey && !e.metaKey && !e.altKey && !e.shiftKey)
            selectMultiple = true;

        this.filter(e.target, selectMultiple);
    },

    _createGraph: function()
    {
        this._containerContentElement = document.createElement("div");
        this._containerContentElement.id = "resources-container-content";
        this.containerElement.appendChild(this._containerContentElement);

        this.summaryBar = new WebInspector.SummaryBar(this.categories);
        this.summaryBar.element.id = "resources-summary";
        this._containerContentElement.appendChild(this.summaryBar.element);

        this.resourcesGraphsElement = document.createElement("div");
        this.resourcesGraphsElement.id = "resources-graphs";
        this._containerContentElement.appendChild(this.resourcesGraphsElement);

        this.dividersElement = document.createElement("div");
        this.dividersElement.id = "resources-dividers";
        this._containerContentElement.appendChild(this.dividersElement);

        this.eventDividersElement = document.createElement("div");
        this.eventDividersElement.id = "resources-event-dividers";
        this._containerContentElement.appendChild(this.eventDividersElement);

        this.dividersLabelBarElement = document.createElement("div");
        this.dividersLabelBarElement.id = "resources-dividers-label-bar";
        this._containerContentElement.appendChild(this.dividersLabelBarElement);
    },

    updateGraphDividersIfNeeded: function(force)
    {
        if (!this.visible) {
            this.needsRefresh = true;
            return false;
        }

        if (document.body.offsetWidth <= 0) {
            // The stylesheet hasn't loaded yet or the window is closed,
            // so we can't calculate what is need. Return early.
            return false;
        }

        var dividerCount = Math.round(this.dividersElement.offsetWidth / 64);
        var slice = this.calculator.boundarySpan / dividerCount;
        if (!force && this._currentDividerSlice === slice)
            return false;

        this._currentDividerSlice = slice;

        this.dividersElement.removeChildren();
        this.eventDividersElement.removeChildren();
        this.dividersLabelBarElement.removeChildren();

        for (var i = 1; i <= dividerCount; ++i) {
            var divider = document.createElement("div");
            divider.className = "resources-divider";
            if (i === dividerCount)
                divider.addStyleClass("last");
            divider.style.left = ((i / dividerCount) * 100) + "%";

            this.dividersElement.appendChild(divider.cloneNode());

            var label = document.createElement("div");
            label.className = "resources-divider-label";
            if (!isNaN(slice))
                label.textContent = this.calculator.formatValue(slice * i);
            divider.appendChild(label);

            this.dividersLabelBarElement.appendChild(divider);
        }
    },

    _updateDividersLabelBarPosition: function()
    {
        var scrollTop = this.containerElement.scrollTop;
        var dividersTop = (scrollTop < this.summaryBar.element.offsetHeight ? this.summaryBar.element.offsetHeight : scrollTop);
        this.dividersElement.style.top = scrollTop + "px";
        this.eventDividersElement.style.top = scrollTop + "px";
        this.dividersLabelBarElement.style.top = dividersTop + "px";
    },

    get needsRefresh()
    {
        return this._needsRefresh;
    },

    set needsRefresh(x)
    {
        if (this._needsRefresh === x)
            return;

        this._needsRefresh = x;

        if (x) {
            if (this.visible && !("_refreshTimeout" in this))
                this._refreshTimeout = setTimeout(this.refresh.bind(this), 500);
        } else {
            if ("_refreshTimeout" in this) {
                clearTimeout(this._refreshTimeout);
                delete this._refreshTimeout;
            }
        }
    },

    refreshIfNeeded: function()
    {
        if (this.needsRefresh)
            this.refresh();
    },

    show: function()
    {
        WebInspector.Panel.prototype.show.call(this);

        this._updateDividersLabelBarPosition();
        this.refreshIfNeeded();
    },

    resize: function()
    {
        this.updateGraphDividersIfNeeded();
    },

    updateMainViewWidth: function(width)
    {
        this._containerContentElement.style.left = width + "px";
        this.updateGraphDividersIfNeeded();
    }
}

WebInspector.AbstractTimelinePanel.prototype.__proto__ = WebInspector.Panel.prototype;

WebInspector.TimelineCalculator = function()
{
}

WebInspector.TimelineCalculator.prototype = {
    computeSummaryValues: function(items)
    {
        var total = 0;
        var categoryValues = {};

        var itemsLength = items.length;
        for (var i = 0; i < itemsLength; ++i) {
            var item = items[i];
            var value = this._value(item);
            if (typeof value === "undefined")
                continue;
            if (!(item.category.name in categoryValues))
                categoryValues[item.category.name] = 0;
            categoryValues[item.category.name] += value;
            total += value;
        }

        return {categoryValues: categoryValues, total: total};
    },

    computeBarGraphPercentages: function(item)
    {
        return {start: 0, middle: 0, end: (this._value(item) / this.boundarySpan) * 100};
    },

    computeBarGraphLabels: function(item)
    {
        const label = this.formatValue(this._value(item));
        return {left: label, right: label, tooltip: label};
    },

    get boundarySpan()
    {
        return this.maximumBoundary - this.minimumBoundary;
    },

    updateBoundaries: function(item)
    {
        this.minimumBoundary = 0;

        var value = this._value(item);
        if (typeof this.maximumBoundary === "undefined" || value > this.maximumBoundary) {
            this.maximumBoundary = value;
            return true;
        }
        return false;
    },

    reset: function()
    {
        delete this.minimumBoundary;
        delete this.maximumBoundary;
    },

    _value: function(item)
    {
        return 0;
    },

    formatValue: function(value)
    {
        return value.toString();
    }
}
