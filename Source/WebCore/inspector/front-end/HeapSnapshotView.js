/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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

/**
 * @constructor
 * @extends {WebInspector.View}
 */
WebInspector.HeapSnapshotView = function(parent, profile)
{
    WebInspector.View.call(this);

    this.element.addStyleClass("heap-snapshot-view");

    this.parent = parent;
    this.parent.addEventListener("profile added", this._updateBaseOptions, this);
    this.parent.addEventListener("profile added", this._updateFilterOptions, this);

    this.viewsContainer = document.createElement("div");
    this.viewsContainer.addStyleClass("views-container");
    this.element.appendChild(this.viewsContainer);

    this.containmentView = new WebInspector.View();
    this.containmentView.element.addStyleClass("view");
    this.containmentDataGrid = new WebInspector.HeapSnapshotContainmentDataGrid();
    this.containmentDataGrid.element.addEventListener("mousedown", this._mouseDownInContentsGrid.bind(this), true);
    this.containmentDataGrid.show(this.containmentView.element);
    this.containmentDataGrid.addEventListener(WebInspector.DataGrid.Events.SelectedNode, this._selectionChanged, this);

    this.constructorsView = new WebInspector.View();
    this.constructorsView.element.addStyleClass("view");
    this.constructorsView.element.appendChild(this._createToolbarWithClassNameFilter());

    this.constructorsDataGrid = new WebInspector.HeapSnapshotConstructorsDataGrid();
    this.constructorsDataGrid.element.addStyleClass("class-view-grid");
    this.constructorsDataGrid.element.addEventListener("mousedown", this._mouseDownInContentsGrid.bind(this), true);
    this.constructorsDataGrid.show(this.constructorsView.element);
    this.constructorsDataGrid.addEventListener(WebInspector.DataGrid.Events.SelectedNode, this._selectionChanged, this);

    this.diffView = new WebInspector.View();
    this.diffView.element.addStyleClass("view");
    this.diffView.element.appendChild(this._createToolbarWithClassNameFilter());

    this.diffDataGrid = new WebInspector.HeapSnapshotDiffDataGrid();
    this.diffDataGrid.element.addStyleClass("class-view-grid");
    this.diffDataGrid.show(this.diffView.element);
    this.diffDataGrid.addEventListener(WebInspector.DataGrid.Events.SelectedNode, this._selectionChanged, this);

    this.dominatorView = new WebInspector.View();
    this.dominatorView.element.addStyleClass("view");
    this.dominatorDataGrid = new WebInspector.HeapSnapshotDominatorsDataGrid();
    this.dominatorDataGrid.element.addEventListener("mousedown", this._mouseDownInContentsGrid.bind(this), true);
    this.dominatorDataGrid.show(this.dominatorView.element);
    this.dominatorDataGrid.addEventListener(WebInspector.DataGrid.Events.SelectedNode, this._selectionChanged, this);

    this.retainmentViewHeader = document.createElement("div");
    this.retainmentViewHeader.addStyleClass("retainers-view-header");
    this.retainmentViewHeader.addEventListener("mousedown", this._startRetainersHeaderDragging.bind(this), true);
    var retainingPathsTitleDiv = document.createElement("div");
    retainingPathsTitleDiv.className = "title";
    var retainingPathsTitle = document.createElement("span");
    retainingPathsTitle.textContent = WebInspector.UIString("Object's retaining tree");
    retainingPathsTitleDiv.appendChild(retainingPathsTitle);
    this.retainmentViewHeader.appendChild(retainingPathsTitleDiv);
    this.element.appendChild(this.retainmentViewHeader);

    this.retainmentView = new WebInspector.View();
    this.retainmentView.element.addStyleClass("view");
    this.retainmentView.element.addStyleClass("retaining-paths-view");
    this.retainmentDataGrid = new WebInspector.HeapSnapshotRetainmentDataGrid();
    this.retainmentDataGrid.show(this.retainmentView.element);
    this.retainmentDataGrid.addEventListener(WebInspector.DataGrid.Events.SelectedNode, this._inspectedObjectChanged, this);
    this.retainmentView.show(this.element);
    this.retainmentDataGrid.reset();

    this.dataGrid = this.constructorsDataGrid;
    this.currentView = this.constructorsView;

    this.viewSelectElement = document.createElement("select");
    this.viewSelectElement.className = "status-bar-item";
    this.viewSelectElement.addEventListener("change", this._changeView.bind(this), false);

    this.views = [{title: "Summary", view: this.constructorsView, grid: this.constructorsDataGrid},
                  {title: "Comparison", view: this.diffView, grid: this.diffDataGrid},
                  {title: "Containment", view: this.containmentView, grid: this.containmentDataGrid},
                  {title: "Dominators", view: this.dominatorView, grid: this.dominatorDataGrid}];
    this.views.current = 0;
    for (var i = 0; i < this.views.length; ++i) {
        var view = this.views[i];
        var option = document.createElement("option");
        option.label = WebInspector.UIString(view.title);
        this.viewSelectElement.appendChild(option);
    }

    this._profileUid = profile.uid;

    this.baseSelectElement = document.createElement("select");
    this.baseSelectElement.className = "status-bar-item hidden";
    this.baseSelectElement.addEventListener("change", this._changeBase.bind(this), false);
    this._updateBaseOptions();

    this.filterSelectElement = document.createElement("select");
    this.filterSelectElement.className = "status-bar-item";
    this.filterSelectElement.addEventListener("change", this._changeFilter.bind(this), false);
    this._updateFilterOptions();

    this.helpButton = new WebInspector.StatusBarButton("", "heap-snapshot-help-status-bar-item status-bar-item");
    this.helpButton.addEventListener("click", this._helpClicked, this);

    this._popoverHelper = new WebInspector.ObjectPopoverHelper(this.element, this._getHoverAnchor.bind(this), this._resolveObjectForPopover.bind(this), undefined, true);

    this._loadProfile(this._profileUid, profileCallback.bind(this));

    function profileCallback()
    {
        var list = this._profiles();
        var profileIndex;
        for (var i = 0; i < list.length; ++i) {
            if (list[i].uid === this._profileUid) {
                profileIndex = i;
                break;
            }
        }

        if (profileIndex > 0)
            this.baseSelectElement.selectedIndex = profileIndex - 1;
        else
            this.baseSelectElement.selectedIndex = profileIndex;
        this.dataGrid.setDataSource(this, this.profileWrapper);
    }
}

WebInspector.HeapSnapshotView.prototype = {
    dispose: function()
    {
        this.profileWrapper.dispose();
        if (this.baseProfile)
            this.baseProfileWrapper.dispose();
        this.containmentDataGrid.dispose();
        this.constructorsDataGrid.dispose();
        this.diffDataGrid.dispose();
        this.dominatorDataGrid.dispose();
        this.retainmentDataGrid.dispose();
    },

    get statusBarItems()
    {
        return [this.viewSelectElement, this.baseSelectElement, this.filterSelectElement, this.helpButton.element];
    },

    get profile()
    {
        return this.parent.getProfile(WebInspector.HeapSnapshotProfileType.TypeId, this._profileUid);
    },

    get profileWrapper()
    {
        return this.profile._proxy;
    },

    get baseProfile()
    {
        return this.parent.getProfile(WebInspector.HeapSnapshotProfileType.TypeId, this._baseProfileUid);
    },

    get baseProfileWrapper()
    {
        return this.baseProfile._proxy;
    },

    wasShown: function()
    {
        if (!this.profileWrapper.loaded)
            this._loadProfile(this._profileUid, profileCallback1.bind(this));
        else
            profileCallback1.call(this);

        function profileCallback1() {
            if (this.baseProfile && !this.baseProfileWrapper.loaded)
                this._loadProfile(this._baseProfileUid, profileCallback2.bind(this));
            else
                profileCallback2.call(this);
        }

        function profileCallback2() {
            this.currentView.show(this.viewsContainer);
        }
    },

    willHide: function()
    {
        this._currentSearchResultIndex = -1;
        this._popoverHelper.hidePopover();
        if (this.helpPopover && this.helpPopover.visible)
            this.helpPopover.hide();
    },

    onResize: function()
    {
        var height = this.retainmentView.element.clientHeight;
        this._updateRetainmentViewHeight(height);
    },

    searchCanceled: function()
    {
        if (this._searchResults) {
            for (var i = 0; i < this._searchResults.length; ++i) {
                var node = this._searchResults[i].node;
                delete node._searchMatched;
                node.refresh();
            }
        }

        delete this._searchFinishedCallback;
        this._currentSearchResultIndex = -1;
        this._searchResults = [];
    },

    performSearch: function(query, finishedCallback)
    {
        // Call searchCanceled since it will reset everything we need before doing a new search.
        this.searchCanceled();

        query = query.trim();

        if (!query.length)
            return;
        if (this.currentView !== this.constructorsView && this.currentView !== this.diffView)
            return;

        this._searchFinishedCallback = finishedCallback;

        function matchesByName(gridNode) {
            return ("name" in gridNode) && gridNode.name.hasSubstring(query, true);
        }

        function matchesById(gridNode) {
            return ("snapshotNodeId" in gridNode) && gridNode.snapshotNodeId === query;
        }

        var matchPredicate;
        if (query.charAt(0) !== "@")
            matchPredicate = matchesByName;
        else {
            query = parseInt(query.substring(1), 10);
            matchPredicate = matchesById;
        }

        function matchesQuery(gridNode)
        {
            delete gridNode._searchMatched;
            if (matchPredicate(gridNode)) {
                gridNode._searchMatched = true;
                gridNode.refresh();
                return true;
            }
            return false;
        }

        var current = this.dataGrid.rootNode().children[0];
        var depth = 0;
        var info = {};

        // Restrict to type nodes and instances.
        const maxDepth = 1;

        while (current) {
            if (matchesQuery(current))
                this._searchResults.push({ node: current });
            current = current.traverseNextNode(false, null, (depth >= maxDepth), info);
            depth += info.depthChange;
        }

        finishedCallback(this, this._searchResults.length);
    },

    jumpToFirstSearchResult: function()
    {
        if (!this._searchResults || !this._searchResults.length)
            return;
        this._currentSearchResultIndex = 0;
        this._jumpToSearchResult(this._currentSearchResultIndex);
    },

    jumpToLastSearchResult: function()
    {
        if (!this._searchResults || !this._searchResults.length)
            return;
        this._currentSearchResultIndex = (this._searchResults.length - 1);
        this._jumpToSearchResult(this._currentSearchResultIndex);
    },

    jumpToNextSearchResult: function()
    {
        if (!this._searchResults || !this._searchResults.length)
            return;
        if (++this._currentSearchResultIndex >= this._searchResults.length)
            this._currentSearchResultIndex = 0;
        this._jumpToSearchResult(this._currentSearchResultIndex);
    },

    jumpToPreviousSearchResult: function()
    {
        if (!this._searchResults || !this._searchResults.length)
            return;
        if (--this._currentSearchResultIndex < 0)
            this._currentSearchResultIndex = (this._searchResults.length - 1);
        this._jumpToSearchResult(this._currentSearchResultIndex);
    },

    showingFirstSearchResult: function()
    {
        return (this._currentSearchResultIndex === 0);
    },

    showingLastSearchResult: function()
    {
        return (this._searchResults && this._currentSearchResultIndex === (this._searchResults.length - 1));
    },

    _jumpToSearchResult: function(index)
    {
        var searchResult = this._searchResults[index];
        if (!searchResult)
            return;

        var node = searchResult.node;
        node.revealAndSelect();
    },

    refreshVisibleData: function()
    {
        var child = this.dataGrid.rootNode().children[0];
        while (child) {
            child.refresh();
            child = child.traverseNextNode(false, null, true);
        }
    },

    _changeBase: function()
    {
        if (this._baseProfileUid === this._profiles()[this.baseSelectElement.selectedIndex].uid)
            return;

        this._baseProfileUid = this._profiles()[this.baseSelectElement.selectedIndex].uid;
        var dataGrid = /** @type {WebInspector.HeapSnapshotDiffDataGrid} */ this.dataGrid;
        this._loadProfile(this._baseProfileUid, dataGrid.setBaseDataSource.bind(dataGrid));

        if (!this.currentQuery || !this._searchFinishedCallback || !this._searchResults)
            return;

        // The current search needs to be performed again. First negate out previous match
        // count by calling the search finished callback with a negative number of matches.
        // Then perform the search again with the same query and callback.
        this._searchFinishedCallback(this, -this._searchResults.length);
        this.performSearch(this.currentQuery, this._searchFinishedCallback);
    },

    _changeFilter: function()
    {
        var profileIndex = this.filterSelectElement.selectedIndex - 1;
        this.dataGrid._filterSelectIndexChanged(this._profiles(), profileIndex);

        if (!this.currentQuery || !this._searchFinishedCallback || !this._searchResults)
            return;

        // The current search needs to be performed again. First negate out previous match
        // count by calling the search finished callback with a negative number of matches.
        // Then perform the search again with the same query and callback.
        this._searchFinishedCallback(this, -this._searchResults.length);
        this.performSearch(this.currentQuery, this._searchFinishedCallback);
    },

    _createToolbarWithClassNameFilter: function()
    {
        var toolbar = document.createElement("div");
        toolbar.addStyleClass("class-view-toolbar");
        var classNameFilter = document.createElement("input");
        classNameFilter.addStyleClass("class-name-filter");
        classNameFilter.setAttribute("placeholder", WebInspector.UIString("Class filter"));
        classNameFilter.addEventListener("keyup", this._changeNameFilter.bind(this, classNameFilter), false);
        toolbar.appendChild(classNameFilter);
        return toolbar;
    },

    _changeNameFilter: function(classNameInputElement)
    {
        var filter = classNameInputElement.value;
        this.dataGrid.changeNameFilter(filter);
    },

    _profiles: function()
    {
        return WebInspector.panels.profiles.getProfiles(WebInspector.HeapSnapshotProfileType.TypeId);
    },

    _loadProfile: function(profileUid, callback)
    {
        WebInspector.panels.profiles.loadHeapSnapshot(profileUid, callback);
    },

    isDetailedSnapshot: function(snapshot)
    {
        var s = new WebInspector.HeapSnapshot(snapshot);
        for (var iter = s.rootNode.edges; iter.hasNext(); iter.next())
            if (iter.edge.node.name === "(GC roots)")
                return true;
        return false;
    },

    processLoadedSnapshot: function(profile, snapshot)
    {
        profile.nodes = snapshot.nodes;
        profile.strings = snapshot.strings;
        var s = new WebInspector.HeapSnapshot(profile);
        profile.sidebarElement.subtitle = Number.bytesToString(s.totalSize);
    },

    _selectionChanged: function(event)
    {
        var selectedNode = event.target.selectedNode;
        this._setRetainmentDataGridSource(selectedNode);
        this._inspectedObjectChanged(event);
    },

    _inspectedObjectChanged: function(event)
    {
        var selectedNode = event.target.selectedNode;
        if (selectedNode instanceof WebInspector.HeapSnapshotGenericObjectNode)
            ConsoleAgent.addInspectedHeapObject(selectedNode.snapshotNodeId);
    },

    _setRetainmentDataGridSource: function(nodeItem)
    {
        if (nodeItem && nodeItem.snapshotNodeIndex)
            this.retainmentDataGrid.setDataSource(this, nodeItem.isDeletedNode ? nodeItem.dataGrid.baseSnapshot : nodeItem.dataGrid.snapshot, nodeItem.snapshotNodeIndex);
        else
            this.retainmentDataGrid.reset();
    },

    _mouseDownInContentsGrid: function(event)
    {
        if (event.detail < 2)
            return;

        var cell = event.target.enclosingNodeOrSelfWithNodeName("td");
        if (!cell || (!cell.hasStyleClass("count-column") && !cell.hasStyleClass("shallowSize-column") && !cell.hasStyleClass("retainedSize-column")))
            return;

        event.consume(true);
    },

    changeView: function(viewTitle, callback)
    {
        var viewIndex = null;
        for (var i = 0; i < this.views.length; ++i)
            if (this.views[i].title === viewTitle) {
                viewIndex = i;
                break;
            }
        if (this.views.current === viewIndex) {
            setTimeout(callback, 0);
            return;
        }
        var grid = this.views[viewIndex].grid;
        function sortingComplete()
        {
            grid.removeEventListener("sorting complete", sortingComplete, this);
            setTimeout(callback, 0);
        }
        this.views[viewIndex].grid.addEventListener("sorting complete", sortingComplete, this);
        this.viewSelectElement.selectedIndex = viewIndex;
        this._changeView({target: {selectedIndex: viewIndex}});
    },

    _changeView: function(event)
    {
        if (!event || !this._profileUid)
            return;
        if (event.target.selectedIndex === this.views.current)
            return;

        this.views.current = event.target.selectedIndex;
        this.currentView.detach();
        var view = this.views[this.views.current];
        this.currentView = view.view;
        this.dataGrid = view.grid;
        this.currentView.show(this.viewsContainer);
        this.refreshVisibleData();
        this.dataGrid.updateWidths();

        if (this.currentView === this.diffView) {
            this.baseSelectElement.removeStyleClass("hidden");
            if (!this.dataGrid.snapshotView) {
                this._changeBase();
                this.dataGrid.setDataSource(this, this.profileWrapper);
            }
        } else {
            this.baseSelectElement.addStyleClass("hidden");
            if (!this.dataGrid.snapshotView)
                this.dataGrid.setDataSource(this, this.profileWrapper);
        }

        if (this.currentView === this.constructorsView)
            this.filterSelectElement.removeStyleClass("hidden");
        else
            this.filterSelectElement.addStyleClass("hidden");

        if (!this.currentQuery || !this._searchFinishedCallback || !this._searchResults)
            return;

        // The current search needs to be performed again. First negate out previous match
        // count by calling the search finished callback with a negative number of matches.
        // Then perform the search again the with same query and callback.
        this._searchFinishedCallback(this, -this._searchResults.length);
        this.performSearch(this.currentQuery, this._searchFinishedCallback);
    },

    _getHoverAnchor: function(target)
    {
        var span = target.enclosingNodeOrSelfWithNodeName("span");
        if (!span)
            return;
        var row = target.enclosingNodeOrSelfWithNodeName("tr");
        if (!row)
            return;
        var gridNode = row._dataGridNode;
        if (!gridNode.hasHoverMessage)
            return;
        span.node = gridNode;
        return span;
    },

    _resolveObjectForPopover: function(element, showCallback, objectGroupName)
    {
        element.node.queryObjectContent(showCallback, objectGroupName);
    },

    _helpClicked: function(event)
    {
        if (!this._helpPopoverContentElement) {
            var refTypes = ["a:", "console-formatted-name", WebInspector.UIString("property"),
                            "0:", "console-formatted-name", WebInspector.UIString("element"),
                            "a:", "console-formatted-number", WebInspector.UIString("context var"),
                            "a:", "console-formatted-null", WebInspector.UIString("system prop")];
            var objTypes = [" a ", "console-formatted-object", "Object",
                            "\"a\"", "console-formatted-string", "String",
                            "/a/", "console-formatted-string", "RegExp",
                            "a()", "console-formatted-function", "Function",
                            "a[]", "console-formatted-object", "Array",
                            "num", "console-formatted-number", "Number",
                            " a ", "console-formatted-null", "System"];

            var contentElement = document.createElement("table");
            contentElement.className = "heap-snapshot-help";
            var headerRow = document.createElement("tr");
            var propsHeader = document.createElement("th");
            propsHeader.textContent = WebInspector.UIString("Property types:");
            headerRow.appendChild(propsHeader);
            var objsHeader = document.createElement("th");
            objsHeader.textContent = WebInspector.UIString("Object types:");
            headerRow.appendChild(objsHeader);
            contentElement.appendChild(headerRow);

            function appendHelp(help, index, cell)
            {
                var div = document.createElement("div");
                div.className = "source-code event-properties";
                var name = document.createElement("span");
                name.textContent = help[index];
                name.className = help[index + 1];
                div.appendChild(name);
                var desc = document.createElement("span");
                desc.textContent = " " + help[index + 2];
                div.appendChild(desc);
                cell.appendChild(div);
            }

            var len = Math.max(refTypes.length, objTypes.length);
            for (var i = 0; i < len; i += 3) {
                var row = document.createElement("tr");
                var refCell = document.createElement("td");
                if (refTypes[i])
                    appendHelp(refTypes, i, refCell);
                row.appendChild(refCell);
                var objCell = document.createElement("td");
                if (objTypes[i])
                    appendHelp(objTypes, i, objCell);
                row.appendChild(objCell);
                contentElement.appendChild(row);
            }
            this._helpPopoverContentElement = contentElement;
            this.helpPopover = new WebInspector.Popover();
        }
        if (this.helpPopover.visible)
            this.helpPopover.hide();
        else
            this.helpPopover.show(this._helpPopoverContentElement, this.helpButton.element);
    },

    _startRetainersHeaderDragging: function(event)
    {
        if (!this.isShowing())
            return;

        WebInspector.elementDragStart(this.retainmentViewHeader, this._retainersHeaderDragging.bind(this), this._endRetainersHeaderDragging.bind(this), event, "row-resize");
        this._previousDragPosition = event.pageY;
        event.consume();
    },

    _retainersHeaderDragging: function(event)
    {
        var height = this.retainmentView.element.clientHeight;
        height += this._previousDragPosition - event.pageY;
        this._previousDragPosition = event.pageY;
        this._updateRetainmentViewHeight(height);
        event.consume(true);
    },

    _endRetainersHeaderDragging: function(event)
    {
        WebInspector.elementDragEnd(event);
        delete this._previousDragPosition;
        event.consume();
    },

    _updateRetainmentViewHeight: function(height)
    {
        height = Number.constrain(height, Preferences.minConsoleHeight, this.element.clientHeight - Preferences.minConsoleHeight);
        this.viewsContainer.style.bottom = (height + this.retainmentViewHeader.clientHeight) + "px";
        this.retainmentView.element.style.height = height + "px";
        this.retainmentViewHeader.style.bottom = height + "px";
    },

    _updateBaseOptions: function()
    {
        var list = this._profiles();
        // We're assuming that snapshots can only be added.
        if (this.baseSelectElement.length === list.length)
            return;

        for (var i = this.baseSelectElement.length, n = list.length; i < n; ++i) {
            var baseOption = document.createElement("option");
            var title = list[i].title;
            if (!title.indexOf(UserInitiatedProfileName))
                title = WebInspector.UIString("Snapshot %d", title.substring(UserInitiatedProfileName.length + 1));
            baseOption.label = title;
            this.baseSelectElement.appendChild(baseOption);
        }
    },

    _updateFilterOptions: function()
    {
        var list = this._profiles();
        // We're assuming that snapshots can only be added.
        if (this.filterSelectElement.length - 1 === list.length)
            return;

        if (!this.filterSelectElement.length) {
            var filterOption = document.createElement("option");
            filterOption.label = WebInspector.UIString("All objects");
            this.filterSelectElement.appendChild(filterOption);
        }

        for (var i = this.filterSelectElement.length - 1, n = list.length; i < n; ++i) {
            var filterOption = document.createElement("option");
            var title = list[i].title;
            if (!title.indexOf(UserInitiatedProfileName)) {
                if (!i)
                    title = WebInspector.UIString("Objects allocated before Snapshot %d", title.substring(UserInitiatedProfileName.length + 1));
                else
                    title = WebInspector.UIString("Objects allocated between Snapshots %d and %d", title.substring(UserInitiatedProfileName.length + 1) - 1, title.substring(UserInitiatedProfileName.length + 1));
            }
            filterOption.label = title;
            this.filterSelectElement.appendChild(filterOption);
        }
    }
};

WebInspector.HeapSnapshotView.prototype.__proto__ = WebInspector.View.prototype;

WebInspector.settings.showHeapSnapshotObjectsHiddenProperties = WebInspector.settings.createSetting("showHeaSnapshotObjectsHiddenProperties", false);

/**
 * @constructor
 * @extends {WebInspector.ProfileType}
 */
WebInspector.HeapSnapshotProfileType = function()
{
    WebInspector.ProfileType.call(this, WebInspector.HeapSnapshotProfileType.TypeId, WebInspector.UIString("Take Heap Snapshot"));
}

WebInspector.HeapSnapshotProfileType.TypeId = "HEAP";

WebInspector.HeapSnapshotProfileType.prototype = {
    get buttonTooltip()
    {
        return WebInspector.UIString("Take heap snapshot.");
    },

    buttonClicked: function()
    {
        WebInspector.panels.profiles.takeHeapSnapshot();
    },

    get treeItemTitle()
    {
        return WebInspector.UIString("HEAP SNAPSHOTS");
    },

    get description()
    {
        return WebInspector.UIString("Heap snapshot profiles show memory distribution among your page's JavaScript objects and related DOM nodes.");
    },

    createSidebarTreeElementForProfile: function(profile)
    {
        return new WebInspector.ProfileSidebarTreeElement(profile, WebInspector.UIString("Snapshot %d"), "heap-snapshot-sidebar-tree-item");
    },

    createView: function(profile)
    {
        return new WebInspector.HeapSnapshotView(WebInspector.panels.profiles, profile);
    },

    /**
     * @override
     * @return {WebInspector.ProfileHeader}
     */
    createTemporaryProfile: function()
    {
        return new WebInspector.ProfileHeader(WebInspector.HeapSnapshotProfileType.TypeId, WebInspector.UIString("Snapshotting\u2026"));
    },

    /**
     * @override
     * @param {ProfilerAgent.ProfileHeader} profile
     * @return {WebInspector.ProfileHeader}
     */
    createProfile: function(profile)
    {
        return new WebInspector.HeapProfileHeader(profile.typeId, profile.title, profile.uid, profile.maxJSObjectId || 0);
    }
}

WebInspector.HeapSnapshotProfileType.prototype.__proto__ = WebInspector.ProfileType.prototype;

/**
 * @constructor
 * @extends {WebInspector.ProfileHeader}
 * @param {string} profileType
 * @param {string} title
 * @param {number} uid
 * @param {number} maxJSObjectId
 */
WebInspector.HeapProfileHeader = function(profileType, title, uid, maxJSObjectId)
{
    WebInspector.ProfileHeader.call(this, profileType, title, uid);
    this.maxJSObjectId = maxJSObjectId;
    this._loaded = false;
    this._totalNumberOfChunks = 0;
}

WebInspector.HeapProfileHeader.prototype = {
    /**
     * @override
     * @param {Function} callback
     */
    load: function(callback)
    {
        if (this._loaded) {
            callback.call(null);
            return;
        }

        if (!this._proxy) {
            function setProfileWait(event) {
                this.sidebarElement.wait = event.data;
            }
            var worker = new WebInspector.HeapSnapshotWorker();
            worker.addEventListener("wait", setProfileWait, this);
            this._proxy = worker.createObject("WebInspector.HeapSnapshotLoader");
        }

        if (this._proxy.startLoading(callback)) {
            this.sidebarElement.subtitle = WebInspector.UIString("Loading\u2026");
            this.sidebarElement.wait = true;
            function done()
            {
                this._loaded = true;
            }
            ProfilerAgent.getProfile(this.typeId, this.uid, done.bind(this));
        }
    },

    /**
     * @param {WebInspector.Event} event
     */
    _saveStatusUpdate: function(event)
    {
        if (event.data !== this._fileName)
            return;
        if (++this._savedChunksCount === this._totalNumberOfChunks) {
            this.sidebarElement.subtitle = Number.bytesToString(this._proxy.totalSize);
            this.sidebarElement.wait = false;
            this._savedChunksCount = 0;
            WebInspector.fileManager.removeEventListener(WebInspector.FileManager.EventTypes.AppendedToURL, this._saveStatusUpdate, this);
        } else
            this.sidebarElement.subtitle = WebInspector.UIString("Saving\u2026 %d\%", Math.floor(this._savedChunksCount * 100 / this._totalNumberOfChunks));
    },

    /**
     * @param {string} chunk
     */
    pushJSONChunk: function(chunk)
    {
        if (this._loaded) {
            this.sidebarElement.wait = true;
            WebInspector.fileManager.append(this._fileName, chunk);
        } else {
            ++this._totalNumberOfChunks;
            this._proxy.pushJSONChunk(chunk);
        }
    },

    finishHeapSnapshot: function()
    {
        function parsed(snapshotProxy)
        {
            this._proxy = snapshotProxy;
            this.sidebarElement.subtitle = Number.bytesToString(snapshotProxy.totalSize);
            this.sidebarElement.wait = false;
            var worker = /** @type {WebInspector.HeapSnapshotWorker} */ snapshotProxy.worker;
            worker.startCheckingForLongRunningCalls();
        }
        if (this._proxy.finishLoading(parsed.bind(this)))
            this.sidebarElement.subtitle = WebInspector.UIString("Parsing\u2026");
    },

    /**
     * @override
     * @return {boolean}
     */
    canSave: function()
    {
        return this._loaded && !this._savedChunksCount && WebInspector.fileManager.canAppend();
    },

    /**
     * @override
     */
    save: function()
    {
        /**
         * @param {WebInspector.Event} event
         */
        function startWritingSnapshot(event)
        {
            if (event.data !== this._fileName)
                return;
            this.sidebarElement.wait = true;
            this.sidebarElement.subtitle = WebInspector.UIString("Saving\u2026 %d\%", 0);
            this._savedChunksCount = 0;
            WebInspector.fileManager.removeEventListener(WebInspector.FileManager.EventTypes.SavedURL, startWritingSnapshot, this);
            WebInspector.fileManager.addEventListener(WebInspector.FileManager.EventTypes.AppendedToURL, this._saveStatusUpdate, this);
            ProfilerAgent.getProfile(this.typeId, this.uid);
        }

        this._fileName = this._fileName || "Heap-" + new Date().toISO8601Compact() + ".json";
        WebInspector.fileManager.addEventListener(WebInspector.FileManager.EventTypes.SavedURL, startWritingSnapshot, this);
        WebInspector.fileManager.save(this._fileName, "", true);
    }
}

WebInspector.HeapProfileHeader.prototype.__proto__ = WebInspector.ProfileHeader.prototype;
