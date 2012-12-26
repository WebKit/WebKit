/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
 * @param {WebInspector.NativeMemoryProfileHeader} profile
 */
WebInspector.NativeMemorySnapshotView = function(profile)
{
    WebInspector.View.call(this);
    this.registerRequiredCSS("nativeMemoryProfiler.css");

    this.element.addStyleClass("native-snapshot-view");
    this._containmentDataGrid = new WebInspector.NativeSnapshotDataGrid(profile);
    this._containmentDataGrid.show(this.element);

    this._heapGraphDataGrid = new WebInspector.NativeHeapGraphDataGrid(profile._graph);

    this._viewSelectElement = document.createElement("select");
    this._viewSelectElement.className = "status-bar-item";
    this._viewSelectElement.addEventListener("change", this._onSelectedViewChanged.bind(this), false);

    this._views = [{title: "Aggregated", view: this._containmentDataGrid},
                  {title: "Graph", view: this._heapGraphDataGrid}];
    this._currentViewIndex = 0;
    for (var i = 0; i < this._views.length; ++i) {
        var view = this._views[i];
        var option = document.createElement("option");
        option.label = WebInspector.UIString(view.title);
        this._viewSelectElement.appendChild(option);
    }
}

WebInspector.NativeMemorySnapshotView.prototype = {
    _onSelectedViewChanged: function(event)
    {
        var index = event.target.selectedIndex;
        if (index === this._currentViewIndex)
            return;

        var currentView = this._views[this._currentViewIndex].view;
        currentView.detach();

        this._currentViewIndex = index;
        var selectedView = this._views[index].view;
        selectedView.show(this.element);
    },

    get statusBarItems()
    {
        var span = document.createElement("span");
        span.className = "status-bar-select-container";
        span.appendChild(this._viewSelectElement);
        return [span];
    },

    __proto__: WebInspector.View.prototype
}



/**
 * @constructor
 * @extends {WebInspector.DataGrid}
 * @param {WebInspector.NativeMemoryProfileHeader} profile
 */
WebInspector.NativeSnapshotDataGrid = function(profile)
{
    var columns = {
        name: { title: WebInspector.UIString("Object"), width: "200px", disclosure: true, sortable: true },
        size: { title: WebInspector.UIString("Size"), sortable: true, sort: "descending" },
    };
    WebInspector.DataGrid.call(this, columns);
    this._profile = profile;
    this._totalNode = new WebInspector.NativeSnapshotNode(profile._memoryBlock, profile._memoryBlock);
    if (WebInspector.settings.showNativeSnapshotUninstrumentedSize.get()) {
        this.setRootNode(new WebInspector.DataGridNode(null, true));
        this.rootNode().appendChild(this._totalNode)
        this._totalNode.expand();
    } else {
        this.setRootNode(this._totalNode);
        this._totalNode._populate();
    }
    this.addEventListener("sorting changed", this.sortingChanged.bind(this), this);
}

WebInspector.NativeSnapshotDataGrid.prototype = {
    sortingChanged: function()
    {
        var expandedNodes = {};
        this._totalNode._storeState(expandedNodes);
        this._totalNode.removeChildren();
        this._totalNode._populate();
        this._totalNode._shouldRefreshChildren = true;
        this._totalNode._restoreState(expandedNodes);
    },

    /**
     * @param {MemoryAgent.MemoryBlock} nodeA
     * @param {MemoryAgent.MemoryBlock} nodeB
     */
    _sortingFunction: function(nodeA, nodeB)
    {
        var sortColumnIdentifier = this.sortColumnIdentifier;
        var sortAscending = this.sortOrder === "ascending";
        var field1 = nodeA[sortColumnIdentifier];
        var field2 = nodeB[sortColumnIdentifier];
        var result = field1 < field2 ? -1 : (field1 > field2 ? 1 : 0);
        if (!sortAscending)
            result = -result;
        return result;
    },

    __proto__: WebInspector.DataGrid.prototype
}

/**
 * @constructor
 * @extends {WebInspector.DataGridNode}
 * @param {MemoryAgent.MemoryBlock} nodeData
 * @param {MemoryAgent.MemoryBlock} rootMemoryBlock
 */
WebInspector.NativeSnapshotNode = function(nodeData, rootMemoryBlock)
{
    this._nodeData = nodeData;
    this._rootMemoryBlock = rootMemoryBlock;
    var viewProperties = WebInspector.MemoryBlockViewProperties._forMemoryBlock(nodeData);
    var data = { name: viewProperties._description, size: this._nodeData.size };
    var hasChildren = !!nodeData.children && nodeData.children.length !== 0;
    WebInspector.DataGridNode.call(this, data, hasChildren);
    this.addEventListener("populate", this._populate, this);
}

WebInspector.NativeSnapshotNode.prototype = {
    /**
     * @override
     * @param {string} columnIdentifier
     * @return {Element}
     */
    createCell: function(columnIdentifier)
    {
        var cell = columnIdentifier === "size" ?
            this._createSizeCell(columnIdentifier) :
            WebInspector.DataGridNode.prototype.createCell.call(this, columnIdentifier);
        return cell;
    },

    /**
     * @param {Object} expandedNodes
     */
    _storeState: function(expandedNodes)
    {
        if (!this.expanded)
            return;
        expandedNodes[this.uid()] = true;
        for (var i in this.children)
            this.children[i]._storeState(expandedNodes);
    },

    /**
     * @param {Object} expandedNodes
     */
    _restoreState: function(expandedNodes)
    {
        if (!expandedNodes[this.uid()])
            return;
        this.expand();
        for (var i in this.children)
            this.children[i]._restoreState(expandedNodes);
    },

    /**
     * @return {string}
     */
    uid: function()
    {
        if (!this._uid)
            this._uid = (!this.parent || !this.parent.uid ? "" : this.parent.uid() || "") + "/" + this._nodeData.name;
        return this._uid;
    },

    /**
     * @param {string} columnIdentifier
     * @return {Element}
     */
    _createSizeCell: function(columnIdentifier)
    {
        var node = this;
        var viewProperties = null;
        var dimmed = false;
        while (!viewProperties || viewProperties._fillStyle === "inherit") {
            viewProperties = WebInspector.MemoryBlockViewProperties._forMemoryBlock(node._nodeData);
            if (viewProperties._fillStyle === "inherit")
                dimmed = true;
            node = node.parent;
        }

        var sizeKB = this._nodeData.size / 1024;
        var totalSize = this._rootMemoryBlock.size;
        var percentage = this._nodeData.size / totalSize  * 100;

        var cell = document.createElement("td");
        cell.className = columnIdentifier + "-column";

        var textDiv = document.createElement("div");
        textDiv.textContent = Number.withThousandsSeparator(sizeKB.toFixed(0)) + "\u2009" + WebInspector.UIString("KB");
        textDiv.className = "size-text";
        cell.appendChild(textDiv);

        var barDiv = document.createElement("div");
        barDiv.className = "size-bar";
        barDiv.style.width = percentage + "%";
        barDiv.style.backgroundColor = viewProperties._fillStyle;
        // fillerDiv displaces percentage text out of the bar visible area if it doesn't fit.
        var fillerDiv = document.createElement("div");
        fillerDiv.className = "percent-text"
        barDiv.appendChild(fillerDiv);
        var percentDiv = document.createElement("div");
        percentDiv.textContent = percentage.toFixed(1) + "%";
        percentDiv.className = "percent-text"
        barDiv.appendChild(percentDiv);

        var barHolderDiv = document.createElement("div");
        if (dimmed)
            barHolderDiv.className = "dimmed";
        barHolderDiv.appendChild(barDiv);
        cell.appendChild(barHolderDiv);

        return cell;
    },

    _populate: function() {
        this.removeEventListener("populate", this._populate, this);
        this._nodeData.children.sort(this.dataGrid._sortingFunction.bind(this.dataGrid));

        for (var node in this._nodeData.children) {
            var nodeData = this._nodeData.children[node];
            this._addChildrenFromGraph(nodeData);
            if (WebInspector.settings.showNativeSnapshotUninstrumentedSize.get() || nodeData.name !== "Other")
                this.appendChild(new WebInspector.NativeSnapshotNode(nodeData, this._rootMemoryBlock));
        }
    },

    /**
     * @param {MemoryAgent.MemoryBlock} memoryBlock
     */
    _addChildrenFromGraph: function(memoryBlock)
    {
        if (memoryBlock.children)
            return;
        if (memoryBlock.name !== "Image" || this._nodeData.name !== "MemoryCache")
            return;

        // Collect objects on the path MemoryCache -> CachedImage -m_image-> BitmapImage -m_frames-> FrameData -m_frame-> SkBitmap -> SkPixelRef
        var graph = this.dataGrid._profile._graph;
        var roots = graph.root().referencedNodes();
        var memoryCache;
        for (var i = 0; i < roots.length; i++) {
            var root = roots[i];
            if (root.className() === "MemoryCache") {
                memoryCache = root;
                break;
            }
        }
        var edges = memoryCache.outgoingEdges();
        var cachedImages = [];
        for (var i = 0; i < edges.length; i++) {
            var target = edges[i].target();
            if (target.className() === "CachedImage") {
                var cachedImage = {
                    name: target.name(),
                    size: target.size(),
                    children: []
                };
                cachedImages.push(cachedImage);
                var image = target.targetOfEdge("m_image");
                if (image.className() === "BitmapImage") {
                    var frames = image.targetsOfAllEdges("m_frame");
                    for (var j = 0; j < frames.length; j++) {
                        var pixels = frames[j].targetOfEdge("pixels");
                        if (pixels) {
                            cachedImage.size += pixels.size();
                            cachedImage.children.push({
                                name: "Bitmap pixels",
                                size: pixels.size()
                            });
                        }
                    }
                }
            }
        }
        memoryBlock.children = cachedImages;
    },

    __proto__: WebInspector.DataGridNode.prototype
}


/**
 * @constructor
 * @extends {WebInspector.DataGrid}
 * @param {WebInspector.NativeHeapGraph} nativeHeapGraph
 */
WebInspector.NativeHeapGraphDataGrid = function(nativeHeapGraph)
{
    var columns = {
        id: { title: WebInspector.UIString("id"), width: "80px", disclosure: true, sortable: true },
        type: { title: WebInspector.UIString("Type"), width: "200px", sortable: true },
        className: { title: WebInspector.UIString("Class name"), width: "200px", sortable: true },
        name: { title: WebInspector.UIString("Name"), width: "200px", sortable: true },
        size: { title: WebInspector.UIString("Size"), sortable: true, sort: "descending" },
    };
    WebInspector.DataGrid.call(this, columns);
    this._nativeHeapGraph = nativeHeapGraph;
    this._root = new WebInspector.NativeHeapGraphDataGridRoot(this._nativeHeapGraph);
    this.setRootNode(this._root);
    this._root._populate();
}

WebInspector.NativeHeapGraphDataGrid.prototype = {
    __proto__: WebInspector.DataGrid.prototype
}


/**
 * @constructor
 * @extends {WebInspector.DataGridNode}
 * @param {WebInspector.NativeHeapGraph} graph
 */
WebInspector.NativeHeapGraphDataGridRoot = function(graph)
{
    WebInspector.DataGridNode.call(this, { id: "root" }, true);
    this._graph = graph;
    this.addEventListener("populate", this._populate, this);
}

WebInspector.NativeHeapGraphDataGridRoot.prototype = {
    _populate: function() {
        this.removeEventListener("populate", this._populate, this);
        var roots = this._graph.root().referencedNodes();
        for (var i = 0; i < roots.length; i++)
            this.appendChild(new WebInspector.NativeHeapGraphDataGridNode(roots[i]));
    },

    __proto__: WebInspector.DataGridNode.prototype
}


/**
 * @constructor
 * @extends {WebInspector.DataGridNode}
 * @param {WebInspector.NativeHeapGraph.Node} node
 */
WebInspector.NativeHeapGraphDataGridNode = function(node)
{
    var data = {
        id: node.id(),
        size: node.size(),
        type: node.type(),
        className: node.className(),
        name: node.name(),
    };
    WebInspector.DataGridNode.call(this, data, node.hasReferencedNodes());
    this._node = node;
    this.addEventListener("populate", this._populate, this);
}

WebInspector.NativeHeapGraphDataGridNode.prototype = {
    _populate: function() {
        this.removeEventListener("populate", this._populate, this);
        var children = this._node.referencedNodes();
        for (var i = 0; i < children.length; i++)
            this.appendChild(new WebInspector.NativeHeapGraphDataGridNode(children[i]));
    },

    __proto__: WebInspector.DataGridNode.prototype
}


/**
 * @constructor
 * @extends {WebInspector.ProfileType}
 */
WebInspector.NativeMemoryProfileType = function()
{
    WebInspector.ProfileType.call(this, WebInspector.NativeMemoryProfileType.TypeId, WebInspector.UIString("Take Native Memory Snapshot"));
    this._nextProfileUid = 1;
}

WebInspector.NativeMemoryProfileType.TypeId = "NATIVE_MEMORY";

WebInspector.NativeMemoryProfileType.prototype = {
    get buttonTooltip()
    {
        return WebInspector.UIString("Take native memory snapshot.");
    },

    /**
     * @override
     * @param {WebInspector.ProfilesPanel} profilesPanel
     * @return {boolean}
     */
    buttonClicked: function(profilesPanel)
    {
        var profileHeader = new WebInspector.NativeMemoryProfileHeader(this, WebInspector.UIString("Snapshot %d", this._nextProfileUid), this._nextProfileUid);
        ++this._nextProfileUid;
        profileHeader.isTemporary = true;
        profilesPanel.addProfileHeader(profileHeader);
        /**
         * @param {?string} error
         * @param {?MemoryAgent.MemoryBlock} memoryBlock
         * @param {?Object=} graph
         */
        function didReceiveMemorySnapshot(error, memoryBlock, graph)
        {
            if (memoryBlock.size && memoryBlock.children) {
                var knownSize = 0;
                for (var i = 0; i < memoryBlock.children.length; i++) {
                    var size = memoryBlock.children[i].size;
                    if (size)
                        knownSize += size;
                }
                var otherSize = memoryBlock.size - knownSize;

                if (otherSize) {
                    memoryBlock.children.push({
                        name: "Other",
                        size: otherSize
                    });
                }
            }
            profileHeader._memoryBlock = memoryBlock;
            profileHeader._graph = new WebInspector.NativeHeapGraph(graph);
            profileHeader.isTemporary = false;
            profileHeader.sidebarElement.subtitle = Number.bytesToString(/** @type{number} */(memoryBlock.size));
        }
        MemoryAgent.getProcessMemoryDistribution(true, didReceiveMemorySnapshot.bind(this));
        return false;
    },

    get treeItemTitle()
    {
        return WebInspector.UIString("MEMORY DISTRIBUTION");
    },

    get description()
    {
        return WebInspector.UIString("Native memory snapshot profiles show memory distribution among browser subsystems");
    },

    /**
     * @override
     * @param {string=} title
     * @return {WebInspector.ProfileHeader}
     */
    createTemporaryProfile: function(title)
    {
        title = title || WebInspector.UIString("Snapshotting\u2026");
        return new WebInspector.NativeMemoryProfileHeader(this, title);
    },

    /**
     * @override
     * @param {ProfilerAgent.ProfileHeader} profile
     * @return {WebInspector.ProfileHeader}
     */
    createProfile: function(profile)
    {
        return new WebInspector.NativeMemoryProfileHeader(this, profile.title, -1);
    },

    __proto__: WebInspector.ProfileType.prototype
}

/**
 * @constructor
 * @extends {WebInspector.ProfileHeader}
 * @param {!WebInspector.NativeMemoryProfileType} type
 * @param {string} title
 * @param {number=} uid
 */
WebInspector.NativeMemoryProfileHeader = function(type, title, uid)
{
    WebInspector.ProfileHeader.call(this, type, title, uid);

    /**
     * @type {MemoryAgent.MemoryBlock}
     */
    this._memoryBlock = null;
}

WebInspector.NativeMemoryProfileHeader.prototype = {
    /**
     * @override
     */
    createSidebarTreeElement: function()
    {
        return new WebInspector.ProfileSidebarTreeElement(this, WebInspector.UIString("Snapshot %d"), "heap-snapshot-sidebar-tree-item");
    },

    /**
     * @override
     * @param {WebInspector.ProfilesPanel} profilesPanel
     */
    createView: function(profilesPanel)
    {
        return new WebInspector.NativeMemorySnapshotView(this);
    },

    __proto__: WebInspector.ProfileHeader.prototype
}

/**
 * @constructor
 * @param {string} fillStyle
 * @param {string} name
 * @param {string} description
 */
WebInspector.MemoryBlockViewProperties = function(fillStyle, name, description)
{
    this._fillStyle = fillStyle;
    this._name = name;
    this._description = description;
}

/**
 * @type {Object.<string, WebInspector.MemoryBlockViewProperties>}
 */
WebInspector.MemoryBlockViewProperties._standardBlocks = null;

WebInspector.MemoryBlockViewProperties._initialize = function()
{
    if (WebInspector.MemoryBlockViewProperties._standardBlocks)
        return;
    WebInspector.MemoryBlockViewProperties._standardBlocks = {};
    function addBlock(fillStyle, name, description)
    {
        WebInspector.MemoryBlockViewProperties._standardBlocks[name] = new WebInspector.MemoryBlockViewProperties(fillStyle, name, WebInspector.UIString(description));
    }
    addBlock("hsl(  0,  0%,  60%)", "ProcessPrivateMemory", "Total");
    addBlock("hsl(  0,  0%,  80%)", "OwnersTypePlaceholder", "OwnersTypePlaceholder");
    addBlock("hsl(  0,  0%,  60%)", "Other", "Other");
    addBlock("hsl(220, 80%,  70%)", "Page", "Page structures");
    addBlock("hsl(100, 60%,  50%)", "JSHeap", "JavaScript heap");
    addBlock("hsl( 90, 40%,  80%)", "JSExternalResources", "JavaScript external resources");
    addBlock("hsl( 90, 60%,  80%)", "JSExternalArrays", "JavaScript external arrays");
    addBlock("hsl( 90, 60%,  80%)", "JSExternalStrings", "JavaScript external strings");
    addBlock("hsl(  0, 80%,  60%)", "WebInspector", "Inspector data");
    addBlock("hsl( 36, 90%,  50%)", "MemoryCache", "Memory cache resources");
    addBlock("hsl( 40, 80%,  80%)", "GlyphCache", "Glyph cache resources");
    addBlock("hsl( 35, 80%,  80%)", "DOMStorageCache", "DOM storage cache");
    addBlock("hsl( 60, 80%,  60%)", "RenderTree", "Render tree");
    addBlock("hsl( 20, 80%,  50%)", "MallocWaste", "Memory allocator waste");
}

WebInspector.MemoryBlockViewProperties._forMemoryBlock = function(memoryBlock)
{
    WebInspector.MemoryBlockViewProperties._initialize();
    var result = WebInspector.MemoryBlockViewProperties._standardBlocks[memoryBlock.name];
    if (result)
        return result;
    return new WebInspector.MemoryBlockViewProperties("inherit", memoryBlock.name, memoryBlock.name);
}


/**
 * @constructor
 * @extends {WebInspector.View}
 */
WebInspector.NativeMemoryBarChart = function()
{
    WebInspector.View.call(this);
    this.registerRequiredCSS("nativeMemoryProfiler.css");
    this._memorySnapshot = null;
    this.element = document.createElement("div");
    this._table = this.element.createChild("table");
    this._divs = {};
    var row = this._table.insertRow();
    this._totalDiv = row.insertCell().createChild("div");
    this._totalDiv.addStyleClass("memory-bar-chart-total");
    row.insertCell();
}

WebInspector.NativeMemoryBarChart.prototype = {
    _updateStats: function()
    {

        /**
         * @param {?string} error
         * @param {?MemoryAgent.MemoryBlock} memoryBlock
         * @param {?Object=} graph
         */
        function didReceiveMemorySnapshot(error, memoryBlock, graph)
        {
            if (memoryBlock.size && memoryBlock.children) {
                var knownSize = 0;
                for (var i = 0; i < memoryBlock.children.length; i++) {
                    var size = memoryBlock.children[i].size;
                    if (size)
                        knownSize += size;
                }
                var otherSize = memoryBlock.size - knownSize;

                if (otherSize) {
                    memoryBlock.children.push({
                        name: "Other",
                        size: otherSize
                    });
                }
            }
            this._memorySnapshot = memoryBlock;
            this._updateView();
        }
        MemoryAgent.getProcessMemoryDistribution(false, didReceiveMemorySnapshot.bind(this));
    },

    /**
     * @override
     */
    willHide: function()
    {
        clearInterval(this._timerId);
    },

    /**
     * @override
     */
    wasShown: function()
    {
        this._timerId = setInterval(this._updateStats.bind(this), 1000);
    },

    _updateView: function()
    {
        var memoryBlock = this._memorySnapshot;
        if (!memoryBlock)
            return;

        var MB = 1024 * 1024;
        var maxSize = 100 * MB;
        for (var i = 0; i < memoryBlock.children.length; ++i)
            maxSize = Math.max(maxSize, memoryBlock.children[i].size);
        var maxBarLength = 500;
        var barLengthSizeRatio = maxBarLength / maxSize;

        for (var i = memoryBlock.children.length - 1; i >= 0 ; --i) {
            var child = memoryBlock.children[i];
            var name = child.name;
            var divs = this._divs[name];
            if (!divs) {
                var row = this._table.insertRow();
                var nameDiv = row.insertCell(-1).createChild("div");
                var viewProperties = WebInspector.MemoryBlockViewProperties._forMemoryBlock(child);
                var title = viewProperties._description;
                nameDiv.textContent = title;
                nameDiv.addStyleClass("memory-bar-chart-name");
                var barCell = row.insertCell(-1);
                var barDiv = barCell.createChild("div");
                barDiv.addStyleClass("memory-bar-chart-bar");
                viewProperties = WebInspector.MemoryBlockViewProperties._forMemoryBlock(child);
                barDiv.style.backgroundColor = viewProperties._fillStyle;
                var unusedDiv = barDiv.createChild("div");
                unusedDiv.addStyleClass("memory-bar-chart-unused");
                var percentDiv = barDiv.createChild("div");
                percentDiv.addStyleClass("memory-bar-chart-percent");
                var sizeDiv = barCell.createChild("div");
                sizeDiv.addStyleClass("memory-bar-chart-size");
                divs = this._divs[name] = { barDiv: barDiv, unusedDiv: unusedDiv, percentDiv: percentDiv, sizeDiv: sizeDiv };
            }
            var unusedSize = 0;
            if (!!child.children) {
                var unusedName = name + ".Unused";
                for (var j = 0; j < child.children.length; ++j) {
                    if (child.children[j].name === unusedName) {
                        unusedSize = child.children[j].size;
                        break;
                    }
                }
            }
            var unusedLength = unusedSize * barLengthSizeRatio;
            var barLength = child.size * barLengthSizeRatio;

            divs.barDiv.style.width = barLength + "px";
            divs.unusedDiv.style.width = unusedLength + "px";
            divs.percentDiv.textContent = barLength > 20 ? (child.size / memoryBlock.size * 100).toFixed(0) + "%" : "";
            divs.sizeDiv.textContent = (child.size / MB).toFixed(1) + "\u2009MB";
        }

        var memoryBlockViewProperties = WebInspector.MemoryBlockViewProperties._forMemoryBlock(memoryBlock);
        this._totalDiv.textContent = memoryBlockViewProperties._description + ": " + (memoryBlock.size / MB).toFixed(1) + "\u2009MB";
    },

    __proto__: WebInspector.View.prototype
}
