/*
 * Copyright (C) 2007, 2008 Apple Inc.  All rights reserved.
 * Copyright (C) 2008 Anthony Ricaud (rik24d@gmail.com)
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

WebInspector.ResourcesPanel = function()
{
    WebInspector.Panel.call(this);

    this.element.addStyleClass("resources");

    this.viewsContainerElement = document.createElement("div");
    this.viewsContainerElement.id = "resource-views";
    this.element.appendChild(this.viewsContainerElement);

    this.containerElement = document.createElement("div");
    this.containerElement.id = "resources-container";
    this.containerElement.addEventListener("scroll", this._updateDividersLabelBarPosition.bind(this), false);
    this.element.appendChild(this.containerElement);

    this.sidebarElement = document.createElement("div");
    this.sidebarElement.id = "resources-sidebar";
    this.sidebarElement.className = "sidebar";
    this.containerElement.appendChild(this.sidebarElement);

    this.sidebarResizeElement = document.createElement("div");
    this.sidebarResizeElement.className = "sidebar-resizer-vertical";
    this.sidebarResizeElement.addEventListener("mousedown", this._startSidebarDragging.bind(this), false);
    this.element.appendChild(this.sidebarResizeElement);

    this.containerContentElement = document.createElement("div");
    this.containerContentElement.id = "resources-container-content";
    this.containerElement.appendChild(this.containerContentElement);

    this.summaryElement = document.createElement("div");
    this.summaryElement.id = "resources-summary";
    this.containerContentElement.appendChild(this.summaryElement);

    this.resourcesGraphsElement = document.createElement("div");
    this.resourcesGraphsElement.id = "resources-graphs";
    this.containerContentElement.appendChild(this.resourcesGraphsElement);

    this.dividersElement = document.createElement("div");
    this.dividersElement.id = "resources-dividers";
    this.containerContentElement.appendChild(this.dividersElement);

    this.dividersLabelBarElement = document.createElement("div");
    this.dividersLabelBarElement.id = "resources-dividers-label-bar";
    this.containerContentElement.appendChild(this.dividersLabelBarElement);

    this.summaryGraphElement = document.createElement("canvas");
    this.summaryGraphElement.setAttribute("width", "450");
    this.summaryGraphElement.setAttribute("height", "38");
    this.summaryGraphElement.id = "resources-summary-graph";
    this.summaryElement.appendChild(this.summaryGraphElement);

    this.legendElement = document.createElement("div");
    this.legendElement.id = "resources-graph-legend";
    this.summaryElement.appendChild(this.legendElement);

    this.sidebarTreeElement = document.createElement("ol");
    this.sidebarTreeElement.className = "sidebar-tree";
    this.sidebarElement.appendChild(this.sidebarTreeElement);

    this.sidebarTree = new TreeOutline(this.sidebarTreeElement);

    var timeGraphItem = new WebInspector.SidebarTreeElement("resources-time-graph-sidebar-item", WebInspector.UIString("Time"));
    timeGraphItem.onselect = this._graphSelected.bind(this);

    var transferTimeCalculator = new WebInspector.ResourceTransferTimeCalculator();
    var transferDurationCalculator = new WebInspector.ResourceTransferDurationCalculator();

    timeGraphItem.sortingOptions = [
        { name: WebInspector.UIString("Sort by Start Time"), sortingFunction: WebInspector.ResourceSidebarTreeElement.CompareByAscendingStartTime, calculator: transferTimeCalculator },
        { name: WebInspector.UIString("Sort by Response Time"), sortingFunction: WebInspector.ResourceSidebarTreeElement.CompareByAscendingResponseReceivedTime, calculator: transferTimeCalculator },
        { name: WebInspector.UIString("Sort by End Time"), sortingFunction: WebInspector.ResourceSidebarTreeElement.CompareByAscendingEndTime, calculator: transferTimeCalculator },
        { name: WebInspector.UIString("Sort by Duration"), sortingFunction: WebInspector.ResourceSidebarTreeElement.CompareByDescendingDuration, calculator: transferDurationCalculator },
        { name: WebInspector.UIString("Sort by Latency"), sortingFunction: WebInspector.ResourceSidebarTreeElement.CompareByDescendingLatency, calculator: transferDurationCalculator },
    ];

    timeGraphItem.selectedSortingOptionIndex = 1;

    var sizeGraphItem = new WebInspector.SidebarTreeElement("resources-size-graph-sidebar-item", WebInspector.UIString("Size"));
    sizeGraphItem.onselect = this._graphSelected.bind(this);

    var transferSizeCalculator = new WebInspector.ResourceTransferSizeCalculator();
    sizeGraphItem.sortingOptions = [
        { name: WebInspector.UIString("Sort by Size"), sortingFunction: WebInspector.ResourceSidebarTreeElement.CompareByDescendingSize, calculator: transferSizeCalculator },
    ];

    sizeGraphItem.selectedSortingOptionIndex = 0;

    this.graphsTreeElement = new WebInspector.SidebarSectionTreeElement(WebInspector.UIString("GRAPHS"), {}, true);
    this.sidebarTree.appendChild(this.graphsTreeElement);

    this.graphsTreeElement.appendChild(timeGraphItem);
    this.graphsTreeElement.appendChild(sizeGraphItem);
    this.graphsTreeElement.expand();

    this.resourcesTreeElement = new WebInspector.SidebarSectionTreeElement(WebInspector.UIString("RESOURCES"), {}, true);
    this.sidebarTree.appendChild(this.resourcesTreeElement);

    this.resourcesTreeElement.expand();

    this.largerResourcesButton = document.createElement("button");
    this.largerResourcesButton.id = "resources-larger-resources-status-bar-item";
    this.largerResourcesButton.className = "status-bar-item toggled-on";
    this.largerResourcesButton.title = WebInspector.UIString("Use small resource rows.");
    this.largerResourcesButton.addEventListener("click", this._toggleLargerResources.bind(this), false);

    this.sortingSelectElement = document.createElement("select");
    this.sortingSelectElement.className = "status-bar-item";
    this.sortingSelectElement.addEventListener("change", this._changeSortingFunction.bind(this), false);

    this.reset();

    timeGraphItem.select();
}

WebInspector.ResourcesPanel.prototype = {
    toolbarItemClass: "resources",

    get toolbarItemLabel()
    {
        return WebInspector.UIString("Resources");
    },

    get statusBarItems()
    {
        return [this.largerResourcesButton, this.sortingSelectElement];
    },

    show: function()
    {
        WebInspector.Panel.prototype.show.call(this);

        this._updateDividersLabelBarPosition();
        this._updateSidebarWidth();
        this.refreshIfNeeded();

        var visibleView = this.visibleView;
        if (visibleView) {
            visibleView.headersVisible = true;
            visibleView.show(this.viewsContainerElement);
        }

        // Hide any views that are visible that are not this panel's current visible view.
        // This can happen when a ResourceView is visible in the Scripts panel then switched
        // to the this panel.
        var resourcesLength = this._resources.length;
        for (var i = 0; i < resourcesLength; ++i) {
            var resource = this._resources[i];
            var view = resource._resourcesView;
            if (!view || view === visibleView)
                continue;
            view.visible = false;
        }
    },

    resize: function()
    {
        this._updateGraphDividersIfNeeded();

        var visibleView = this.visibleView;
        if (visibleView && "resize" in visibleView)
            visibleView.resize();
    },

    get searchableViews()
    {
        var views = [];

        const visibleView = this.visibleView;
        if (visibleView && visibleView.performSearch)
            views.push(visibleView);

        var resourcesLength = this._resources.length;
        for (var i = 0; i < resourcesLength; ++i) {
            var resource = this._resources[i];
            if (!resource._resourcesTreeElement)
                continue;
            var resourceView = this.resourceViewForResource(resource);
            if (!resourceView.performSearch || resourceView === visibleView)
                continue;
            views.push(resourceView);
        }

        return views;
    },

    get searchResultsSortFunction()
    {
        const resourceTreeElementSortFunction = this.sortingFunction;

        function sortFuction(a, b)
        {
            return resourceTreeElementSortFunction(a.resource._resourcesTreeElement, b.resource._resourcesTreeElement);
        }

        return sortFuction;
    },

    searchMatchFound: function(view, matches)
    {
        view.resource._resourcesTreeElement.searchMatches = matches;
    },

    searchCanceled: function(startingNewSearch)
    {
        WebInspector.Panel.prototype.searchCanceled.call(this, startingNewSearch);

        if (startingNewSearch || !this._resources)
            return;

        for (var i = 0; i < this._resources.length; ++i) {
            var resource = this._resources[i];
            if (resource._resourcesTreeElement)
                resource._resourcesTreeElement.updateErrorsAndWarnings();
        }
    },

    performSearch: function(query)
    {
        for (var i = 0; i < this._resources.length; ++i) {
            var resource = this._resources[i];
            if (resource._resourcesTreeElement)
                resource._resourcesTreeElement.resetBubble();
        }

        WebInspector.Panel.prototype.performSearch.call(this, query);
    },

    get visibleView()
    {
        if (this.visibleResource)
            return this.visibleResource._resourcesView;
        return null;
    },

    get calculator()
    {
        return this._calculator;
    },

    set calculator(x)
    {
        if (!x || this._calculator === x)
            return;

        this._calculator = x;
        this._calculator.reset();

        this._staleResources = this._resources;
        this.refresh();
    },

    get sortingFunction()
    {
        return this._sortingFunction;
    },

    set sortingFunction(x)
    {
        this._sortingFunction = x;
        this._sortResourcesIfNeeded();
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

    refresh: function()
    {
        this.needsRefresh = false;

        var staleResourcesLength = this._staleResources.length;
        var boundariesChanged = false;

        for (var i = 0; i < staleResourcesLength; ++i) {
            var resource = this._staleResources[i];
            if (!resource._resourcesTreeElement) {
                // Create the resource tree element and graph.
                resource._resourcesTreeElement = new WebInspector.ResourceSidebarTreeElement(resource);
                resource._resourcesTreeElement._resourceGraph = new WebInspector.ResourceGraph(resource);

                this.resourcesTreeElement.appendChild(resource._resourcesTreeElement);
                this.resourcesGraphsElement.appendChild(resource._resourcesTreeElement._resourceGraph.graphElement);
            }

            resource._resourcesTreeElement.refresh();

            if (this.calculator.updateBoundaries(resource))
                boundariesChanged = true;
        }

        if (boundariesChanged) {
            // The boundaries changed, so all resource graphs are stale.
            this._staleResources = this._resources;
            staleResourcesLength = this._staleResources.length;
        }

        for (var i = 0; i < staleResourcesLength; ++i)
            this._staleResources[i]._resourcesTreeElement._resourceGraph.refresh(this.calculator);

        this._staleResources = [];

        this._updateGraphDividersIfNeeded();
        this._sortResourcesIfNeeded();
        this._updateSummaryGraph();
    },

    reset: function()
    {
        this.closeVisibleResource();

        this.containerElement.scrollTop = 0;

        delete this.currentQuery;
        this.searchCanceled();

        if (this._calculator)
            this._calculator.reset();

        if (this._resources) {
            var resourcesLength = this._resources.length;
            for (var i = 0; i < resourcesLength; ++i) {
                var resource = this._resources[i];

                resource.warnings = 0;
                resource.errors = 0;

                delete resource._resourcesTreeElement;
                delete resource._resourcesView;
            }
        }

        this._resources = [];
        this._staleResources = [];

        this.resourcesTreeElement.removeChildren();
        this.viewsContainerElement.removeChildren();
        this.resourcesGraphsElement.removeChildren();
        this.legendElement.removeChildren();

        this._updateGraphDividersIfNeeded(true);

        this._drawSummaryGraph(); // draws an empty graph
    },

    addResource: function(resource)
    {
        this._resources.push(resource);
        this.refreshResource(resource);
    },

    removeResource: function(resource)
    {
        if (this.visibleView === resource._resourcesView)
            this.closeVisibleResource();

        this._resources.remove(resource, true);

        if (resource._resourcesTreeElement) {
            this.resourcesTreeElement.removeChild(resource._resourcesTreeElement);
            this.resourcesGraphsElement.removeChild(resource._resourcesTreeElement._resourceGraph.graphElement);
        }

        resource.warnings = 0;
        resource.errors = 0;

        delete resource._resourcesTreeElement;
        delete resource._resourcesView;

        this._adjustScrollPosition();
    },

    addMessageToResource: function(resource, msg)
    {
        if (!resource)
            return;

        switch (msg.level) {
        case WebInspector.ConsoleMessage.MessageLevel.Warning:
            resource.warnings += msg.repeatDelta;
            break;
        case WebInspector.ConsoleMessage.MessageLevel.Error:
            resource.errors += msg.repeatDelta;
            break;
        }

        if (!this.currentQuery && resource._resourcesTreeElement)
            resource._resourcesTreeElement.updateErrorsAndWarnings();

        var view = this.resourceViewForResource(resource);
        if (view.addMessage)
            view.addMessage(msg);
    },

    clearMessages: function()
    {
        var resourcesLength = this._resources.length;
        for (var i = 0; i < resourcesLength; ++i) {
            var resource = this._resources[i];
            resource.warnings = 0;
            resource.errors = 0;

            if (!this.currentQuery && resource._resourcesTreeElement)
                resource._resourcesTreeElement.updateErrorsAndWarnings();

            var view = resource._resourcesView;
            if (!view || !view.clearMessages)
                continue;
            view.clearMessages();
        }
    },

    refreshResource: function(resource)
    {
        this._staleResources.push(resource);
        this.needsRefresh = true;
    },

    recreateViewForResourceIfNeeded: function(resource)
    {
        if (!resource || !resource._resourcesView)
            return;

        var newView = this._createResourceView(resource);
        if (newView.prototype === resource._resourcesView.prototype)
            return;

        resource.warnings = 0;
        resource.errors = 0;

        if (!this.currentQuery && resource._resourcesTreeElement)
            resource._resourcesTreeElement.updateErrorsAndWarnings();

        var oldView = resource._resourcesView;

        resource._resourcesView.detach();
        delete resource._resourcesView;

        resource._resourcesView = newView;

        newView.headersVisible = oldView.headersVisible;

        if (oldView.visible && oldView.element.parentNode)
            newView.show(oldView.element.parentNode);
    },

    showResource: function(resource, line)
    {
        if (!resource)
            return;

        this.containerElement.addStyleClass("viewing-resource");

        if (this.visibleResource && this.visibleResource._resourcesView)
            this.visibleResource._resourcesView.hide();

        var view = this.resourceViewForResource(resource);
        view.headersVisible = true;
        view.show(this.viewsContainerElement);

        if (line) {
            if (view.revealLine)
                view.revealLine(line);
            if (view.highlightLine)
                view.highlightLine(line);
        }

        if (resource._resourcesTreeElement) {
            resource._resourcesTreeElement.reveal();
            resource._resourcesTreeElement.select(true);
        }

        this.visibleResource = resource;

        this._updateSidebarWidth();
    },

    showView: function(view)
    {
        if (!view)
            return;
        this.showResource(view.resource);
    },

    closeVisibleResource: function()
    {
        this.containerElement.removeStyleClass("viewing-resource");
        this._updateDividersLabelBarPosition();

        if (this.visibleResource && this.visibleResource._resourcesView)
            this.visibleResource._resourcesView.hide();
        delete this.visibleResource;

        if (this._lastSelectedGraphTreeElement)
            this._lastSelectedGraphTreeElement.select(true);

        this._updateSidebarWidth();
    },

    resourceViewForResource: function(resource)
    {
        if (!resource)
            return null;
        if (!resource._resourcesView)
            resource._resourcesView = this._createResourceView(resource);
        return resource._resourcesView;
    },

    sourceFrameForResource: function(resource)
    {
        var view = this.resourceViewForResource(resource);
        if (!view)
            return null;

        if (!view.setupSourceFrameIfNeeded)
            return null;

        // Setting up the source frame requires that we be attached.
        if (!this.element.parentNode)
            this.attach();

        view.setupSourceFrameIfNeeded();
        return view.sourceFrame;
    },

    handleKeyEvent: function(event)
    {
        this.sidebarTree.handleKeyEvent(event);
    },

    _makeLegendElement: function(label, value, color)
    {
        var legendElement = document.createElement("label");
        legendElement.className = "resources-graph-legend-item";

        if (color) {
            var swatch = document.createElement("canvas");
            swatch.className = "resources-graph-legend-swatch";
            swatch.setAttribute("width", "13");
            swatch.setAttribute("height", "24");

            legendElement.appendChild(swatch);

            this._drawSwatch(swatch, color);
        }

        var labelElement = document.createElement("div");
        labelElement.className = "resources-graph-legend-label";
        legendElement.appendChild(labelElement);

        var headerElement = document.createElement("div");
        var headerElement = document.createElement("div");
        headerElement.className = "resources-graph-legend-header";
        headerElement.textContent = label;
        labelElement.appendChild(headerElement);

        var valueElement = document.createElement("div");
        valueElement.className = "resources-graph-legend-value";
        valueElement.textContent = value;
        labelElement.appendChild(valueElement);

        return legendElement;
    },

    _sortResourcesIfNeeded: function()
    {
        var sortedElements = [].concat(this.resourcesTreeElement.children);
        sortedElements.sort(this.sortingFunction);

        var sortedElementsLength = sortedElements.length;
        for (var i = 0; i < sortedElementsLength; ++i) {
            var treeElement = sortedElements[i];
            if (treeElement === this.resourcesTreeElement.children[i])
                continue;

            var wasSelected = treeElement.selected;
            this.resourcesTreeElement.removeChild(treeElement);
            this.resourcesTreeElement.insertChild(treeElement, i);
            if (wasSelected)
                treeElement.select(true);

            var graphElement = treeElement._resourceGraph.graphElement;
            this.resourcesGraphsElement.insertBefore(graphElement, this.resourcesGraphsElement.children[i]);
        }
    },

    _updateGraphDividersIfNeeded: function(force)
    {
        if (!this.visible) {
            this.needsRefresh = true;
            return;
        }

        if (document.body.offsetWidth <= 0) {
            // The stylesheet hasn't loaded yet or the window is closed,
            // so we can't calculate what is need. Return early.
            return;
        }

        var dividerCount = Math.round(this.dividersElement.offsetWidth / 64);
        var slice = this.calculator.boundarySpan / dividerCount;
        if (!force && this._currentDividerSlice === slice)
            return;

        this._currentDividerSlice = slice;

        this.dividersElement.removeChildren();
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

    _fadeOutRect: function(ctx, x, y, w, h, a1, a2)
    {
        ctx.save();

        var gradient = ctx.createLinearGradient(x, y, x, y + h);
        gradient.addColorStop(0.0, "rgba(0, 0, 0, " + (1.0 - a1) + ")");
        gradient.addColorStop(0.8, "rgba(0, 0, 0, " + (1.0 - a2) + ")");
        gradient.addColorStop(1.0, "rgba(0, 0, 0, 1.0)");

        ctx.globalCompositeOperation = "destination-out";

        ctx.fillStyle = gradient;
        ctx.fillRect(x, y, w, h);

        ctx.restore();
    },

    _drawSwatch: function(canvas, color)
    {
        var ctx = canvas.getContext("2d");

        function drawSwatchSquare() {
            ctx.fillStyle = color;
            ctx.fillRect(0, 0, 13, 13);

            var gradient = ctx.createLinearGradient(0, 0, 13, 13);
            gradient.addColorStop(0.0, "rgba(255, 255, 255, 0.2)");
            gradient.addColorStop(1.0, "rgba(255, 255, 255, 0.0)");

            ctx.fillStyle = gradient;
            ctx.fillRect(0, 0, 13, 13);

            gradient = ctx.createLinearGradient(13, 13, 0, 0);
            gradient.addColorStop(0.0, "rgba(0, 0, 0, 0.2)");
            gradient.addColorStop(1.0, "rgba(0, 0, 0, 0.0)");

            ctx.fillStyle = gradient;
            ctx.fillRect(0, 0, 13, 13);

            ctx.strokeStyle = "rgba(0, 0, 0, 0.6)";
            ctx.strokeRect(0.5, 0.5, 12, 12);
        }

        ctx.clearRect(0, 0, 13, 24);

        drawSwatchSquare();

        ctx.save();

        ctx.translate(0, 25);
        ctx.scale(1, -1);

        drawSwatchSquare();

        ctx.restore();

        this._fadeOutRect(ctx, 0, 13, 13, 13, 0.5, 0.0);
    },

    _drawSummaryGraph: function(segments)
    {
        if (!this.summaryGraphElement)
            return;

        if (!segments || !segments.length) {
            segments = [{color: "white", value: 1}];
            this._showingEmptySummaryGraph = true;
        } else
            delete this._showingEmptySummaryGraph;

        // Calculate the total of all segments.
        var total = 0;
        for (var i = 0; i < segments.length; ++i)
            total += segments[i].value;

        // Calculate the percentage of each segment, rounded to the nearest percent.
        var percents = segments.map(function(s) { return Math.max(Math.round(100 * s.value / total), 1) });

        // Calculate the total percentage.
        var percentTotal = 0;
        for (var i = 0; i < percents.length; ++i)
            percentTotal += percents[i];

        // Make sure our percentage total is not greater-than 100, it can be greater
        // if we rounded up for a few segments.
        while (percentTotal > 100) {
            for (var i = 0; i < percents.length && percentTotal > 100; ++i) {
                if (percents[i] > 1) {
                    --percents[i];
                    --percentTotal;
                }
            }
        }

        // Make sure our percentage total is not less-than 100, it can be less
        // if we rounded down for a few segments.
        while (percentTotal < 100) {
            for (var i = 0; i < percents.length && percentTotal < 100; ++i) {
                ++percents[i];
                ++percentTotal;
            }
        }

        var ctx = this.summaryGraphElement.getContext("2d");

        var x = 0;
        var y = 0;
        var w = 450;
        var h = 19;
        var r = (h / 2);

        function drawPillShadow()
        {
            // This draws a line with a shadow that is offset away from the line. The line is stroked
            // twice with different X shadow offsets to give more feathered edges. Later we erase the
            // line with destination-out 100% transparent black, leaving only the shadow. This only
            // works if nothing has been drawn into the canvas yet.

            ctx.beginPath();
            ctx.moveTo(x + 4, y + h - 3 - 0.5);
            ctx.lineTo(x + w - 4, y + h - 3 - 0.5);
            ctx.closePath();

            ctx.save();

            ctx.shadowBlur = 2;
            ctx.shadowColor = "rgba(0, 0, 0, 0.5)";
            ctx.shadowOffsetX = 3;
            ctx.shadowOffsetY = 5;

            ctx.strokeStyle = "white";
            ctx.lineWidth = 1;

            ctx.stroke();

            ctx.shadowOffsetX = -3;

            ctx.stroke();

            ctx.restore();

            ctx.save();

            ctx.globalCompositeOperation = "destination-out";
            ctx.strokeStyle = "rgba(0, 0, 0, 1)";
            ctx.lineWidth = 1;

            ctx.stroke();

            ctx.restore();
        }

        function drawPill()
        {
            // Make a rounded rect path.
            ctx.beginPath();
            ctx.moveTo(x, y + r);
            ctx.lineTo(x, y + h - r);
            ctx.quadraticCurveTo(x, y + h, x + r, y + h);
            ctx.lineTo(x + w - r, y + h);
            ctx.quadraticCurveTo(x + w, y + h, x + w, y + h - r);
            ctx.lineTo(x + w, y + r);
            ctx.quadraticCurveTo(x + w, y, x + w - r, y);
            ctx.lineTo(x + r, y);
            ctx.quadraticCurveTo(x, y, x, y + r);
            ctx.closePath();

            // Clip to the rounded rect path.
            ctx.save();
            ctx.clip();

            // Fill the segments with the associated color.
            var previousSegmentsWidth = 0;
            for (var i = 0; i < segments.length; ++i) {
                var segmentWidth = Math.round(w * percents[i] / 100);
                ctx.fillStyle = segments[i].color;
                ctx.fillRect(x + previousSegmentsWidth, y, segmentWidth, h);
                previousSegmentsWidth += segmentWidth;
            }

            // Draw the segment divider lines.
            ctx.lineWidth = 1;
            for (var i = 1; i < 20; ++i) {
                ctx.beginPath();
                ctx.moveTo(x + (i * Math.round(w / 20)) + 0.5, y);
                ctx.lineTo(x + (i * Math.round(w / 20)) + 0.5, y + h);
                ctx.closePath();

                ctx.strokeStyle = "rgba(0, 0, 0, 0.2)";
                ctx.stroke();

                ctx.beginPath();
                ctx.moveTo(x + (i * Math.round(w / 20)) + 1.5, y);
                ctx.lineTo(x + (i * Math.round(w / 20)) + 1.5, y + h);
                ctx.closePath();

                ctx.strokeStyle = "rgba(255, 255, 255, 0.2)";
                ctx.stroke();
            }

            // Draw the pill shading.
            var lightGradient = ctx.createLinearGradient(x, y, x, y + (h / 1.5));
            lightGradient.addColorStop(0.0, "rgba(220, 220, 220, 0.6)");
            lightGradient.addColorStop(0.4, "rgba(220, 220, 220, 0.2)");
            lightGradient.addColorStop(1.0, "rgba(255, 255, 255, 0.0)");

            var darkGradient = ctx.createLinearGradient(x, y + (h / 3), x, y + h);
            darkGradient.addColorStop(0.0, "rgba(0, 0, 0, 0.0)");
            darkGradient.addColorStop(0.8, "rgba(0, 0, 0, 0.2)");
            darkGradient.addColorStop(1.0, "rgba(0, 0, 0, 0.5)");

            ctx.fillStyle = darkGradient;
            ctx.fillRect(x, y, w, h);

            ctx.fillStyle = lightGradient;
            ctx.fillRect(x, y, w, h);

            ctx.restore();
        }

        ctx.clearRect(x, y, w, (h * 2));

        drawPillShadow();
        drawPill();

        ctx.save();

        ctx.translate(0, (h * 2) + 1);
        ctx.scale(1, -1);

        drawPill();

        ctx.restore();

        this._fadeOutRect(ctx, x, y + h + 1, w, h, 0.5, 0.0);
    },

    _updateSummaryGraph: function()
    {
        var graphInfo = this.calculator.computeSummaryValues(this._resources);

        var categoryOrder = ["documents", "stylesheets", "images", "scripts", "xhr", "fonts", "other"];
        var categoryColors = {documents: {r: 47, g: 102, b: 236}, stylesheets: {r: 157, g: 231, b: 119}, images: {r: 164, g: 60, b: 255}, scripts: {r: 255, g: 121, b: 0}, xhr: {r: 231, g: 231, b: 10}, fonts: {r: 255, g: 82, b: 62}, other: {r: 186, g: 186, b: 186}};
        var fillSegments = [];

        this.legendElement.removeChildren();

        for (var i = 0; i < categoryOrder.length; ++i) {
            var category = categoryOrder[i];
            var size = graphInfo.categoryValues[category];
            if (!size)
                continue;

            var color = categoryColors[category];
            var colorString = "rgb(" + color.r + ", " + color.g + ", " + color.b + ")";

            var fillSegment = {color: colorString, value: size};
            fillSegments.push(fillSegment);

            var legendLabel = this._makeLegendElement(WebInspector.resourceCategories[category].title, this.calculator.formatValue(size), colorString);
            this.legendElement.appendChild(legendLabel);
        }

        if (graphInfo.total) {
            var totalLegendLabel = this._makeLegendElement(WebInspector.UIString("Total"), this.calculator.formatValue(graphInfo.total));
            totalLegendLabel.addStyleClass("total");
            this.legendElement.appendChild(totalLegendLabel);
        }

        this._drawSummaryGraph(fillSegments);
    },

    _updateDividersLabelBarPosition: function()
    {
        var scrollTop = this.containerElement.scrollTop;
        var dividersTop = (scrollTop < this.summaryElement.offsetHeight ? this.summaryElement.offsetHeight : scrollTop);
        this.dividersElement.style.top = scrollTop + "px";
        this.dividersLabelBarElement.style.top = dividersTop + "px";
    },

    _graphSelected: function(treeElement)
    {
        if (this._lastSelectedGraphTreeElement)
            this._lastSelectedGraphTreeElement.selectedSortingOptionIndex = this.sortingSelectElement.selectedIndex;

        this._lastSelectedGraphTreeElement = treeElement;

        this.sortingSelectElement.removeChildren();
        for (var i = 0; i < treeElement.sortingOptions.length; ++i) {
            var sortingOption = treeElement.sortingOptions[i];
            var option = document.createElement("option");
            option.label = sortingOption.name;
            option.sortingFunction = sortingOption.sortingFunction;
            option.calculator = sortingOption.calculator;
            this.sortingSelectElement.appendChild(option);
        }

        this.sortingSelectElement.selectedIndex = treeElement.selectedSortingOptionIndex;
        this._changeSortingFunction();

        this.closeVisibleResource();
        this.containerElement.scrollTop = 0;
    },

    _toggleLargerResources: function()
    {
        if (!this.resourcesTreeElement._childrenListNode)
            return;

        this.resourcesTreeElement.smallChildren = !this.resourcesTreeElement.smallChildren;

        if (this.resourcesTreeElement.smallChildren) {
            this.resourcesGraphsElement.addStyleClass("small");
            this.largerResourcesButton.title = WebInspector.UIString("Use large resource rows.");
            this.largerResourcesButton.removeStyleClass("toggled-on");
            this._adjustScrollPosition();
        } else {
            this.resourcesGraphsElement.removeStyleClass("small");
            this.largerResourcesButton.title = WebInspector.UIString("Use small resource rows.");
            this.largerResourcesButton.addStyleClass("toggled-on");
        }
    },

    _adjustScrollPosition: function()
    {
        // Prevent the container from being scrolled off the end.
        if ((this.containerElement.scrollTop + this.containerElement.offsetHeight) > this.sidebarElement.offsetHeight)
            this.containerElement.scrollTop = (this.sidebarElement.offsetHeight - this.containerElement.offsetHeight);
    },

    _changeSortingFunction: function()
    {
        var selectedOption = this.sortingSelectElement[this.sortingSelectElement.selectedIndex];
        this.sortingFunction = selectedOption.sortingFunction;
        this.calculator = selectedOption.calculator;
    },

    _createResourceView: function(resource)
    {
        switch (resource.category) {
            case WebInspector.resourceCategories.documents:
            case WebInspector.resourceCategories.stylesheets:
            case WebInspector.resourceCategories.scripts:
            case WebInspector.resourceCategories.xhr:
                return new WebInspector.SourceView(resource);
            case WebInspector.resourceCategories.images:
                return new WebInspector.ImageView(resource);
            case WebInspector.resourceCategories.fonts:
                return new WebInspector.FontView(resource);
            default:
                return new WebInspector.ResourceView(resource);
        }
    },

    _startSidebarDragging: function(event)
    {
        WebInspector.elementDragStart(this.sidebarResizeElement, this._sidebarDragging.bind(this), this._endSidebarDragging.bind(this), event, "col-resize");
    },

    _sidebarDragging: function(event)
    {
        this._updateSidebarWidth(event.pageX);

        event.preventDefault();
    },

    _endSidebarDragging: function(event)
    {
        WebInspector.elementDragEnd(event);
    },

    _updateSidebarWidth: function(width)
    {
        if (this.sidebarElement.offsetWidth <= 0) {
            // The stylesheet hasn't loaded yet or the window is closed,
            // so we can't calculate what is need. Return early.
            return;
        }

        if (!("_currentSidebarWidth" in this))
            this._currentSidebarWidth = this.sidebarElement.offsetWidth;

        if (typeof width === "undefined")
            width = this._currentSidebarWidth;

        width = Number.constrain(width, Preferences.minSidebarWidth, window.innerWidth / 2);

        this._currentSidebarWidth = width;

        if (this.visibleResource) {
            this.containerElement.style.width = width + "px";
            this.sidebarElement.style.removeProperty("width");
        } else {
            this.sidebarElement.style.width = width + "px";
            this.containerElement.style.removeProperty("width");
        }

        this.containerContentElement.style.left = width + "px";
        this.viewsContainerElement.style.left = width + "px";
        this.sidebarResizeElement.style.left = (width - 3) + "px";

        this._updateGraphDividersIfNeeded();

        var visibleView = this.visibleView;
        if (visibleView && "resize" in visibleView)
            visibleView.resize();
    }
}

WebInspector.ResourcesPanel.prototype.__proto__ = WebInspector.Panel.prototype;

WebInspector.ResourceCalculator = function()
{
}

WebInspector.ResourceCalculator.prototype = {
    computeSummaryValues: function(resources)
    {
        var total = 0;
        var categoryValues = {};

        var resourcesLength = resources.length;
        for (var i = 0; i < resourcesLength; ++i) {
            var resource = resources[i];
            var value = this._value(resource);
            if (typeof value === "undefined")
                continue;
            if (!(resource.category.name in categoryValues))
                categoryValues[resource.category.name] = 0;
            categoryValues[resource.category.name] += value;
            total += value;
        }

        return {categoryValues: categoryValues, total: total};
    },

    computeBarGraphPercentages: function(resource)
    {
        return {start: 0, middle: 0, end: (this._value(resource) / this.boundarySpan) * 100};
    },

    computeBarGraphLabels: function(resource)
    {
        const label = this.formatValue(this._value(resource));
        var tooltip = label;
        if (resource.cached)
            tooltip = WebInspector.UIString("%s (from cache)", tooltip);
        return {left: label, right: label, tooltip: tooltip};
    },

    get boundarySpan()
    {
        return this.maximumBoundary - this.minimumBoundary;
    },

    updateBoundaries: function(resource)
    {
        this.minimumBoundary = 0;

        var value = this._value(resource);
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

    _value: function(resource)
    {
        return 0;
    },

    formatValue: function(value)
    {
        return value.toString();
    }
}

WebInspector.ResourceTimeCalculator = function(startAtZero)
{
    WebInspector.ResourceCalculator.call(this);
    this.startAtZero = startAtZero;
}

WebInspector.ResourceTimeCalculator.prototype = {
    computeSummaryValues: function(resources)
    {
        var resourcesByCategory = {};
        var resourcesLength = resources.length;
        for (var i = 0; i < resourcesLength; ++i) {
            var resource = resources[i];
            if (!(resource.category.name in resourcesByCategory))
                resourcesByCategory[resource.category.name] = [];
            resourcesByCategory[resource.category.name].push(resource);
        }

        var earliestStart;
        var latestEnd;
        var categoryValues = {};
        for (var category in resourcesByCategory) {
            resourcesByCategory[category].sort(WebInspector.Resource.CompareByTime);
            categoryValues[category] = 0;

            var segment = {start: -1, end: -1};

            var categoryResources = resourcesByCategory[category];
            var resourcesLength = categoryResources.length;
            for (var i = 0; i < resourcesLength; ++i) {
                var resource = categoryResources[i];
                if (resource.startTime === -1 || resource.endTime === -1)
                    continue;

                if (typeof earliestStart === "undefined")
                    earliestStart = resource.startTime;
                else
                    earliestStart = Math.min(earliestStart, resource.startTime);

                if (typeof latestEnd === "undefined")
                    latestEnd = resource.endTime;
                else
                    latestEnd = Math.max(latestEnd, resource.endTime);

                if (resource.startTime <= segment.end) {
                    segment.end = Math.max(segment.end, resource.endTime);
                    continue;
                }

                categoryValues[category] += segment.end - segment.start;

                segment.start = resource.startTime;
                segment.end = resource.endTime;
            }

            // Add the last segment
            categoryValues[category] += segment.end - segment.start;
        }

        return {categoryValues: categoryValues, total: latestEnd - earliestStart};
    },

    computeBarGraphPercentages: function(resource)
    {
        if (resource.startTime !== -1)
            var start = ((resource.startTime - this.minimumBoundary) / this.boundarySpan) * 100;
        else
            var start = 0;

        if (resource.responseReceivedTime !== -1)
            var middle = ((resource.responseReceivedTime - this.minimumBoundary) / this.boundarySpan) * 100;
        else
            var middle = (this.startAtZero ? start : 100);

        if (resource.endTime !== -1)
            var end = ((resource.endTime - this.minimumBoundary) / this.boundarySpan) * 100;
        else
            var end = (this.startAtZero ? middle : 100);

        if (this.startAtZero) {
            end -= start;
            middle -= start;
            start = 0;
        }

        return {start: start, middle: middle, end: end};
    },

    computeBarGraphLabels: function(resource)
    {
        var leftLabel = "";
        if (resource.latency > 0)
            leftLabel = this.formatValue(resource.latency);

        var rightLabel = "";
        if (resource.responseReceivedTime !== -1 && resource.endTime !== -1)
            rightLabel = this.formatValue(resource.endTime - resource.responseReceivedTime);

        if (leftLabel && rightLabel) {
            var total = this.formatValue(resource.duration);
            var tooltip = WebInspector.UIString("%s latency, %s download (%s total)", leftLabel, rightLabel, total);
        } else if (leftLabel)
            var tooltip = WebInspector.UIString("%s latency", leftLabel);
        else if (rightLabel)
            var tooltip = WebInspector.UIString("%s download", rightLabel);

        if (resource.cached)
            tooltip = WebInspector.UIString("%s (from cache)", tooltip);

        return {left: leftLabel, right: rightLabel, tooltip: tooltip};
    },

    updateBoundaries: function(resource)
    {
        var didChange = false;

        var lowerBound;
        if (this.startAtZero)
            lowerBound = 0;
        else
            lowerBound = this._lowerBound(resource);

        if (lowerBound !== -1 && (typeof this.minimumBoundary === "undefined" || lowerBound < this.minimumBoundary)) {
            this.minimumBoundary = lowerBound;
            didChange = true;
        }

        var upperBound = this._upperBound(resource);
        if (upperBound !== -1 && (typeof this.maximumBoundary === "undefined" || upperBound > this.maximumBoundary)) {
            this.maximumBoundary = upperBound;
            didChange = true;
        }

        return didChange;
    },

    formatValue: function(value)
    {
        return Number.secondsToString(value, WebInspector.UIString.bind(WebInspector));
    },

    _lowerBound: function(resource)
    {
        return 0;
    },

    _upperBound: function(resource)
    {
        return 0;
    },
}

WebInspector.ResourceTimeCalculator.prototype.__proto__ = WebInspector.ResourceCalculator.prototype;

WebInspector.ResourceTransferTimeCalculator = function()
{
    WebInspector.ResourceTimeCalculator.call(this, false);
}

WebInspector.ResourceTransferTimeCalculator.prototype = {
    formatValue: function(value)
    {
        return Number.secondsToString(value, WebInspector.UIString.bind(WebInspector));
    },

    _lowerBound: function(resource)
    {
        return resource.startTime;
    },

    _upperBound: function(resource)
    {
        return resource.endTime;
    }
}

WebInspector.ResourceTransferTimeCalculator.prototype.__proto__ = WebInspector.ResourceTimeCalculator.prototype;

WebInspector.ResourceTransferDurationCalculator = function()
{
    WebInspector.ResourceTimeCalculator.call(this, true);
}

WebInspector.ResourceTransferDurationCalculator.prototype = {
    formatValue: function(value)
    {
        return Number.secondsToString(value, WebInspector.UIString.bind(WebInspector));
    },

    _upperBound: function(resource)
    {
        return resource.duration;
    }
}

WebInspector.ResourceTransferDurationCalculator.prototype.__proto__ = WebInspector.ResourceTimeCalculator.prototype;

WebInspector.ResourceTransferSizeCalculator = function()
{
    WebInspector.ResourceCalculator.call(this);
}

WebInspector.ResourceTransferSizeCalculator.prototype = {
    _value: function(resource)
    {
        return resource.contentLength;
    },

    formatValue: function(value)
    {
        return Number.bytesToString(value, WebInspector.UIString.bind(WebInspector));
    }
}

WebInspector.ResourceTransferSizeCalculator.prototype.__proto__ = WebInspector.ResourceCalculator.prototype;

WebInspector.ResourceSidebarTreeElement = function(resource)
{
    this.resource = resource;

    this.createIconElement();

    WebInspector.SidebarTreeElement.call(this, "resource-sidebar-tree-item", "", "", resource);

    this.refreshTitles();
}

WebInspector.ResourceSidebarTreeElement.prototype = {
    onattach: function()
    {
        WebInspector.SidebarTreeElement.prototype.onattach.call(this);

        this._listItemNode.addStyleClass("resources-category-" + this.resource.category.name);
    },

    onselect: function()
    {
        WebInspector.panels.resources.showResource(this.resource);
    },

    get mainTitle()
    {
        return this.resource.displayName;
    },

    set mainTitle(x)
    {
        // Do nothing.
    },

    get subtitle()
    {
        var subtitle = this.resource.displayDomain;

        if (this.resource.path && this.resource.lastPathComponent) {
            var lastPathComponentIndex = this.resource.path.lastIndexOf("/" + this.resource.lastPathComponent);
            if (lastPathComponentIndex != -1)
                subtitle += this.resource.path.substring(0, lastPathComponentIndex);
        }

        return subtitle;
    },

    set subtitle(x)
    {
        // Do nothing.
    },

    createIconElement: function()
    {
        var previousIconElement = this.iconElement;

        if (this.resource.category === WebInspector.resourceCategories.images) {
            var previewImage = document.createElement("img");
            previewImage.className = "image-resource-icon-preview";
            previewImage.src = this.resource.url;

            this.iconElement = document.createElement("div");
            this.iconElement.className = "icon";
            this.iconElement.appendChild(previewImage);
        } else {
            this.iconElement = document.createElement("img");
            this.iconElement.className = "icon";
        }

        if (previousIconElement)
            previousIconElement.parentNode.replaceChild(this.iconElement, previousIconElement);
    },

    refresh: function()
    {
        this.refreshTitles();

        if (!this._listItemNode.hasStyleClass("resources-category-" + this.resource.category.name)) {
            this._listItemNode.removeMatchingStyleClasses("resources-category-\\w+");
            this._listItemNode.addStyleClass("resources-category-" + this.resource.category.name);

            this.createIconElement();
        }
    },

    resetBubble: function()
    {
        this.bubbleText = "";
        this.bubbleElement.removeStyleClass("search-matches");
        this.bubbleElement.removeStyleClass("warning");
        this.bubbleElement.removeStyleClass("error");
    },

    set searchMatches(matches)
    {
        this.resetBubble();

        if (!matches)
            return;

        this.bubbleText = matches;
        this.bubbleElement.addStyleClass("search-matches");
    },

    updateErrorsAndWarnings: function()
    {
        this.resetBubble();

        if (this.resource.warnings || this.resource.errors)
            this.bubbleText = (this.resource.warnings + this.resource.errors);

        if (this.resource.warnings)
            this.bubbleElement.addStyleClass("warning");

        if (this.resource.errors)
            this.bubbleElement.addStyleClass("error");
    }
}

WebInspector.ResourceSidebarTreeElement.CompareByAscendingStartTime = function(a, b)
{
    return WebInspector.Resource.CompareByStartTime(a.resource, b.resource)
        || WebInspector.Resource.CompareByEndTime(a.resource, b.resource)
        || WebInspector.Resource.CompareByResponseReceivedTime(a.resource, b.resource);
}

WebInspector.ResourceSidebarTreeElement.CompareByAscendingResponseReceivedTime = function(a, b)
{
    return WebInspector.Resource.CompareByResponseReceivedTime(a.resource, b.resource)
        || WebInspector.Resource.CompareByStartTime(a.resource, b.resource)
        || WebInspector.Resource.CompareByEndTime(a.resource, b.resource);
}

WebInspector.ResourceSidebarTreeElement.CompareByAscendingEndTime = function(a, b)
{
    return WebInspector.Resource.CompareByEndTime(a.resource, b.resource)
        || WebInspector.Resource.CompareByStartTime(a.resource, b.resource)
        || WebInspector.Resource.CompareByResponseReceivedTime(a.resource, b.resource);
}

WebInspector.ResourceSidebarTreeElement.CompareByDescendingDuration = function(a, b)
{
    return -1 * WebInspector.Resource.CompareByDuration(a.resource, b.resource);
}

WebInspector.ResourceSidebarTreeElement.CompareByDescendingLatency = function(a, b)
{
    return -1 * WebInspector.Resource.CompareByLatency(a.resource, b.resource);
}

WebInspector.ResourceSidebarTreeElement.CompareByDescendingSize = function(a, b)
{
    return -1 * WebInspector.Resource.CompareBySize(a.resource, b.resource);
}

WebInspector.ResourceSidebarTreeElement.prototype.__proto__ = WebInspector.SidebarTreeElement.prototype;

WebInspector.ResourceGraph = function(resource)
{
    this.resource = resource;

    this._graphElement = document.createElement("div");
    this._graphElement.className = "resources-graph-side";
    this._graphElement.addEventListener("mouseover", this.refreshLabelPositions.bind(this), false);

    if (resource.cached)
        this._graphElement.addStyleClass("resource-cached");

    this._barAreaElement = document.createElement("div");
    this._barAreaElement.className = "resources-graph-bar-area hidden";
    this._graphElement.appendChild(this._barAreaElement);

    this._barLeftElement = document.createElement("div");
    this._barLeftElement.className = "resources-graph-bar waiting";
    this._barAreaElement.appendChild(this._barLeftElement);

    this._barRightElement = document.createElement("div");
    this._barRightElement.className = "resources-graph-bar";
    this._barAreaElement.appendChild(this._barRightElement);

    this._labelLeftElement = document.createElement("div");
    this._labelLeftElement.className = "resources-graph-label waiting";
    this._barAreaElement.appendChild(this._labelLeftElement);

    this._labelRightElement = document.createElement("div");
    this._labelRightElement.className = "resources-graph-label";
    this._barAreaElement.appendChild(this._labelRightElement);

    this._graphElement.addStyleClass("resources-category-" + resource.category.name);
}

WebInspector.ResourceGraph.prototype = {
    get graphElement()
    {
        return this._graphElement;
    },

    refreshLabelPositions: function()
    {
        this._labelLeftElement.style.removeProperty("left");
        this._labelLeftElement.style.removeProperty("right");
        this._labelLeftElement.removeStyleClass("before");
        this._labelLeftElement.removeStyleClass("hidden");

        this._labelRightElement.style.removeProperty("left");
        this._labelRightElement.style.removeProperty("right");
        this._labelRightElement.removeStyleClass("after");
        this._labelRightElement.removeStyleClass("hidden");

        const labelPadding = 10;
        const rightBarWidth = (this._barRightElement.offsetWidth - labelPadding);
        const leftBarWidth = ((this._barLeftElement.offsetWidth - this._barRightElement.offsetWidth) - labelPadding);

        var labelBefore = (this._labelLeftElement.offsetWidth > leftBarWidth);
        var labelAfter = (this._labelRightElement.offsetWidth > rightBarWidth);

        if (labelBefore) {
            if ((this._graphElement.offsetWidth * (this._percentages.start / 100)) < (this._labelLeftElement.offsetWidth + 10))
                this._labelLeftElement.addStyleClass("hidden");
            this._labelLeftElement.style.setProperty("right", (100 - this._percentages.start) + "%");
            this._labelLeftElement.addStyleClass("before");
        } else {
            this._labelLeftElement.style.setProperty("left", this._percentages.start + "%");
            this._labelLeftElement.style.setProperty("right", (100 - this._percentages.middle) + "%");
        }

        if (labelAfter) {
            if ((this._graphElement.offsetWidth * ((100 - this._percentages.end) / 100)) < (this._labelRightElement.offsetWidth + 10))
                this._labelRightElement.addStyleClass("hidden");
            this._labelRightElement.style.setProperty("left", this._percentages.end + "%");
            this._labelRightElement.addStyleClass("after");
        } else {
            this._labelRightElement.style.setProperty("left", this._percentages.middle + "%");
            this._labelRightElement.style.setProperty("right", (100 - this._percentages.end) + "%");
        }
    },

    refresh: function(calculator)
    {
        var percentages = calculator.computeBarGraphPercentages(this.resource);
        var labels = calculator.computeBarGraphLabels(this.resource);

        this._percentages = percentages;

        this._barAreaElement.removeStyleClass("hidden");

        if (!this._graphElement.hasStyleClass("resources-category-" + this.resource.category.name)) {
            this._graphElement.removeMatchingStyleClasses("resources-category-\\w+");
            this._graphElement.addStyleClass("resources-category-" + this.resource.category.name);
        }

        this._barLeftElement.style.setProperty("left", percentages.start + "%");
        this._barLeftElement.style.setProperty("right", (100 - percentages.end) + "%");

        this._barRightElement.style.setProperty("left", percentages.middle + "%");
        this._barRightElement.style.setProperty("right", (100 - percentages.end) + "%");

        this._labelLeftElement.textContent = labels.left;
        this._labelRightElement.textContent = labels.right;

        var tooltip = (labels.tooltip || "");
        this._barLeftElement.title = tooltip;
        this._labelLeftElement.title = tooltip;
        this._labelRightElement.title = tooltip;
        this._barRightElement.title = tooltip;
    }
}
