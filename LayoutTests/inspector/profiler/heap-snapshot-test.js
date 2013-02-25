var initialize_HeapSnapshotTest = function() {

InspectorTest.createJSHeapSnapshotMockObject = function()
{
    return {
        _rootNodeIndex: 0,
        _nodeTypeOffset: 0,
        _nodeNameOffset: 1,
        _nodeEdgeCountOffset: 2,
        _nodeFieldCount: 3,
        _edgeFieldsCount: 3,
        _edgeTypeOffset: 0,
        _edgeNameOffset: 1,
        _edgeToNodeOffset: 2,
        _nodeTypes: ["hidden", "object"],
        _edgeTypes: ["element", "property", "shortcut"],
        _edgeShortcutType: -1,
        _edgeHiddenType: -1,
        _edgeElementType: 0,
        _realNodesLength: 18,
        // Represents the following graph:
        //   (numbers in parentheses indicate node offset)
        // 
        //         -> A (3) --ac- C (9) -ce- E(15)
        //       a/   |          /
        //  "" (0)    1    - bc -
        //       b\   v   /
        //         -> B (6) -bd- D (12)
        //
        _nodes: [
            0, 0, 2,    //  0: root
            1, 1, 2,    //  3: A
            1, 2, 2,    //  6: B
            1, 3, 1,    //  9: C
            1, 4, 0,    // 12: D
            1, 5, 0],   // 15: E
        _containmentEdges: [
            2,  6, 3,   //  0: shortcut 'a' to node 'A'
            1,  7, 6,   //  3: property 'b' to node 'B'
            0,  1, 6,   //  6: element '1' to node 'B'
            1,  8, 9,   //  9: property 'ac' to node 'C'
            1,  9, 9,   // 12: property 'bc' to node 'C'
            1, 10, 12,  // 15: property 'bd' to node 'D'
            1, 11, 15], // 18: property 'ce' to node 'E'
        _strings: ["", "A", "B", "C", "D", "E", "a", "b", "ac", "bc", "bd", "ce"],
        _firstEdgeIndexes: [0, 6, 12, 18, 21, 21, 21],
        createNode: WebInspector.JSHeapSnapshot.prototype.createNode,
        createEdge: WebInspector.JSHeapSnapshot.prototype.createEdge,
        createRetainingEdge: WebInspector.JSHeapSnapshot.prototype.createRetainingEdge
    };
};

InspectorTest.createHeapSnapshotMockRaw = function()
{
    // Effectively the same graph as in createJSHeapSnapshotMockObject,
    // but having full set of fields.
    //
    // A triple in parentheses indicates node index, self size and
    // retained size.
    //
    //          --- A (14,2,2) --ac- C (40,4,10) -ce- E(57,6,6)
    //         /    |                /
    //  "" (1,0,20) 1       --bc-----
    //         \    v      /
    //          --- B (27,3,8) --bd- D (50,5,5)
    //
    return {
        snapshot: {
            meta: {
                node_fields: ["type", "name", "id", "self_size", "retained_size", "dominator", "edge_count"],
                node_types: [["hidden", "object"], "", "", "", "", "", ""],
                edge_fields: ["type", "name_or_index", "to_node"],
                edge_types: [["element", "property", "shortcut"], "", ""]
            },
            node_count: 6,
            edge_count: 7},
        nodes: [
            0, 0, 1, 0, 20,  0, 2,  // root (0)
            1, 1, 2, 2,  2,  0, 2,  // A (7)
            1, 2, 3, 3,  8,  0, 2,  // B (14)
            1, 3, 4, 4, 10,  0, 1,  // C (21)
            1, 4, 5, 5,  5, 14, 0,  // D (28)
            1, 5, 6, 6,  6, 21, 0], // E (35)
        edges: [
            // root node edges
            2,  6,  7, // shortcut 'a' to node 'A'
            1,  7, 14, // property 'b' to node 'B'

            // A node edges
            0,  1, 14, // element 1 to node 'B'
            1,  8, 21, // property 'ac' to node 'C'

            // B node edges
            1,  9, 21, // property 'bc' to node 'C'
            1, 10, 28, // property 'bd' to node 'D'

            // C node edges
            1, 11, 35], // property 'ce' to node 'E'
        strings: ["", "A", "B", "C", "D", "E", "a", "b", "ac", "bc", "bd", "ce"]
    };
};

InspectorTest._postprocessHeapSnapshotMock = function(mock)
{
    mock.nodes = new Uint32Array(mock.nodes);
    mock.edges = new Uint32Array(mock.edges);
    return mock;
};

InspectorTest.createHeapSnapshotMock = function()
{
    return InspectorTest._postprocessHeapSnapshotMock(InspectorTest.createHeapSnapshotMockRaw());
};

InspectorTest.createHeapSnapshotMockWithDOM = function()
{
    return InspectorTest._postprocessHeapSnapshotMock({
        snapshot: {
            meta: {
                node_fields: ["type", "name", "id", "edge_count"],
                node_types: [["hidden", "object"], "", "", ""],
                edge_fields: ["type", "name_or_index", "to_node"],
                edge_types: [["element", "hidden", "internal"], "", ""]
            },
            node_count: 13,
            edge_count: 13
        },
        nodes: [
            // A tree with Window objects.
            //
            //    |----->Window--->A
            //    |             \
            //    |----->Window--->B--->C
            //    |        |     \
            //  (root)   hidden   --->D--internal / "native"-->N
            //    |         \         |
            //    |----->E   H     internal
            //    |                   v
            //    |----->F--->G       M
            //
            /* (root) */    0,  0,  1, 4,
            /* Window */    1, 11,  2, 2,
            /* Window */    1, 11,  3, 3,
            /* E */         1,  5,  4, 0,
            /* F */         1,  6,  5, 1,
            /* A */         1,  1,  6, 0,
            /* B */         1,  2,  7, 1,
            /* D */         1,  4,  8, 2,
            /* H */         1,  8,  9, 0,
            /* G */         1,  7, 10, 0,
            /* C */         1,  3, 11, 0,
            /* N */         1, 10, 12, 0,
            /* M */         1,  9, 13, 0
            ],
        edges: [
            /* from (root) */  0,  1,  4, 0, 2,  8, 0, 3, 12, 0, 4, 16,
            /* from Window */  0,  1, 20, 0, 2, 24,
            /* from Window */  0,  1, 24, 0, 2, 28, 1, 3, 32,
            /* from F */       0,  1, 36,
            /* from B */       0,  1, 40,
            /* from D */       2, 12, 44, 2, 1, 48
            ],
        strings: ["", "A", "B", "C", "D", "E", "F", "G", "H", "M", "N", "Window", "native"]
    });
};

InspectorTest.startProfilerTest = function(callback)
{
    WebInspector.showPanel("profiles");
    WebInspector.settings.showHeapSnapshotObjectsHiddenProperties.set(true);

    function profilerEnabled()
    {
        InspectorTest.addResult("Profiler was enabled.");
        // We mock out HeapProfilerAgent -- as DRT runs in single-process mode, Inspector
        // and test share the same heap. Taking a snapshot takes too long for a test,
        // so we provide synthetic snapshots.
        InspectorTest._panelReset = InspectorTest.override(WebInspector.panels.profiles, "_reset", function(){}, true);
        InspectorTest.addSniffer(WebInspector.HeapSnapshotView.prototype, "show", InspectorTest._snapshotViewShown, true);

        detailedHeapProfilesEnabled();
    }

    function detailedHeapProfilesEnabled()
    {
        // Reduce the number of populated nodes to speed up testing.
        WebInspector.HeapSnapshotContainmentDataGrid.prototype.defaultPopulateCount = function() { return 10; };
        WebInspector.HeapSnapshotConstructorsDataGrid.prototype.defaultPopulateCount = function() { return 10; };
        WebInspector.HeapSnapshotDiffDataGrid.prototype.defaultPopulateCount = function() { return 5; };
        WebInspector.HeapSnapshotDominatorsDataGrid.prototype.defaultPopulateCount = function() { return 3; }
        InspectorTest.addResult("Detailed heap profiles were enabled.");
        InspectorTest.safeWrap(callback)();
    }

    if (WebInspector.panels.profiles._profilerEnabled)
        profilerEnabled();
    else {
        InspectorTest.addSniffer(WebInspector.panels.profiles, "_profilerWasEnabled", profilerEnabled);
        WebInspector.panels.profiles._toggleProfiling(false);
    }
};

InspectorTest.completeProfilerTest = function()
{
    // There is no way to disable detailed heap profiles.

    function completeTest()
    {
        InspectorTest.addResult("");
        InspectorTest.addResult("Profiler was disabled.");
        InspectorTest.completeTest();
    }

    var profilesPanel = WebInspector.panels.profiles;
    if (!profilesPanel._profilerEnabled)
        completeTest();
    else {
        InspectorTest.addSniffer(WebInspector.panels.profiles, "_profilerWasDisabled", completeTest);
        profilesPanel._toggleProfiling(false);
    }
};

InspectorTest.runHeapSnapshotTestSuite = function(testSuite)
{
    if (!Capabilities.heapProfilerPresent) {
        InspectorTest.addResult("Heap profiler is disabled");
        InspectorTest.completeTest();
        return;
    }

    InspectorTest._nextUid = 1;
    var testSuiteTests = testSuite.slice();

    function runner()
    {
        if (!testSuiteTests.length) {
            InspectorTest.completeProfilerTest();
            return;
        }

        var nextTest = testSuiteTests.shift();
        InspectorTest.addResult("");
        InspectorTest.addResult("Running: " + /function\s([^(]*)/.exec(nextTest)[1]);
        InspectorTest._panelReset.call(WebInspector.panels.profiles);
        InspectorTest.safeWrap(nextTest)(runner, runner);
    }

    InspectorTest.startProfilerTest(runner);
};

InspectorTest.assertColumnContentsEqual = function(reference, actual)
{
    var length = Math.min(reference.length, actual.length);
    for (var i = 0; i < length; ++i)
        InspectorTest.assertEquals(reference[i], actual[i], "row " + i);
    if (reference.length > length)
        InspectorTest.addResult("extra rows in reference array:\n" + reference.slice(length).join("\n"));
    else if (actual.length > length)
        InspectorTest.addResult("extra rows in actual array:\n" + actual.slice(length).join("\n"));
};

InspectorTest.checkArrayIsSorted = function(contents, sortType, sortOrder)
{
    function simpleComparator(a, b)
    {
        return a < b ? -1 : (a > b ? 1 : 0);
    }
    function parseSize(size)
    {
        if (size.substr(0, 1) === '"') size = JSON.parse(size);
        // Remove thousands separator.
        return parseInt(size.replace(/[\u2009,]/g, ""));
    }
    function extractField(data, field)
    {
        if (data.substr(0, 1) !== "{") return data;
        data = JSON.parse(data);
        if (!data[field])
            InspectorTest.addResult("No " + field + " field in " + JSON.stringify(data));
        return data[field];
    }
    function extractId(data)
    {
        return parseInt(extractField(data, "nodeId"));
    }
    var extractor = {
        text: function (data) { return extractField(data, "value"); },
        number: function (data) { return parseInt(data, 10); },
        size: function (data) { return parseSize(data); },
        name: function (data) { return extractField(data, "name"); },
        id: function (data) { return extractId(data); }
    }[sortType];
    var acceptableComparisonResult = {
        ascending: -1,
        descending: 1
    }[sortOrder];

    if (!extractor) {
        InspectorTest.addResult("Invalid sort type: " + sortType);
        return;
    }
    if (!acceptableComparisonResult) {
        InspectorTest.addResult("Invalid sort order: " + sortOrder);
        return;
    }

    for (var i = 0; i < contents.length - 1; ++i) {
        var a = extractor(contents[i]);
        var b = extractor(contents[i + 1]);
        var result = simpleComparator(a, b);
        if (result !== 0 && result !== acceptableComparisonResult)
            InspectorTest.addResult("Elements " + i + " and " + (i + 1) + " are out of order: " + a + " " + b + " (" + sortOrder + ")");
    }
};

InspectorTest.clickColumn = function(column, callback)
{
    callback = InspectorTest.safeWrap(callback);
    var cell = this._currentGrid()._headerTableHeaders[column.identifier];
    var event = { target: { enclosingNodeOrSelfWithNodeName: function() { return cell; } } };

    function sortingComplete()
    {
        InspectorTest._currentGrid().removeEventListener("sorting complete", sortingComplete, this);
        InspectorTest.assertEquals(column.identifier, this._currentGrid().sortColumnIdentifier, "unexpected sorting");
        column.sort = this._currentGrid().sortOrder;
        function callCallback()
        {
            callback(column);
        }
        setTimeout(callCallback, 0);
    }
    InspectorTest._currentGrid().addEventListener("sorting complete", sortingComplete, this);
    this._currentGrid()._clickInHeaderCell(event);
};

InspectorTest.clickRowAndGetRetainers = function(row, callback)
{
    callback = InspectorTest.safeWrap(callback);
    var event = {
        target: {
            enclosingNodeOrSelfWithNodeName: function() { return row._element; },
            selectedNode: row
        }
    };
    this._currentGrid()._mouseDownInDataTable(event);
    var rootNode = InspectorTest._currentProfileView().retainmentDataGrid.rootNode();
    function populateComplete()
    {
        rootNode.removeEventListener("populate complete", populateComplete, this);
        callback(rootNode);
    }
    rootNode.addEventListener("populate complete", populateComplete, this);
};

InspectorTest.clickShowMoreButton = function(buttonName, row, callback)
{
    callback = InspectorTest.safeWrap(callback);
    var parent = row.parent;
    function populateComplete()
    {
        parent.removeEventListener("populate complete", populateComplete, this);
        function callCallback()
        {
            callback(parent);
        }
        setTimeout(callCallback, 0);
    }
    parent.addEventListener("populate complete", populateComplete, this);
    row[buttonName].click();
};

InspectorTest.columnContents = function(column, row)
{
    // Make sure invisible nodes are removed from the view port.
    this._currentGrid().updateVisibleNodes();
    var result = [];
    var parent = row || this._currentGrid().rootNode();
    for (var node = parent.children[0]; node; node = node.traverseNextNode(true, parent, true)) {
        if (!node.selectable)
            continue;
        var data = node.data[column.identifier];
        if (typeof data === "object")
            data = JSON.stringify(data);
        result.push(data);
    }
    return result;
};

InspectorTest.countDataRows = function(row, filter)
{
    var result = 0;
    filter = filter || function(node) { return node.selectable; };
    for (var node = row.children[0]; node; node = node.traverseNextNode(true, row, true)) {
        if (filter(node))
            ++result;
    }
    return result;
};

InspectorTest.HeapNode = function(name, selfSize, type, id)
{
    this._type = type || InspectorTest.HeapNode.Type.object;
    this._name = name;
    this._selfSize = selfSize || 0;
    this._builder = null;
    this._edges = {};
    this._edgesCount = 0;
    this._id = id;
}

InspectorTest.HeapNode.Type = {
    "hidden": "hidden",
    "array": "array",
    "string": "string",
    "object": "object",
    "code": "code",
    "closure": "closure",
    "regexp": "regexp",
    "number": "number",
    "native": "native"
};

InspectorTest.HeapNode.prototype = {
    linkNode: function(node, type, nameOrIndex)
    {
        if (!this._builder)
            throw new Error("parent node is not connected to a snapshot");

        if (!node._builder)
            node._setBuilder(this._builder);

        if (nameOrIndex === undefined)
            nameOrIndex = this._edgesCount;
        ++this._edgesCount;

        if (nameOrIndex in this._edges)
            throw new Error("Can't add edge with the same nameOrIndex. nameOrIndex: " + nameOrIndex + " oldNodeName: " + this._edges[nameOrIndex]._name + " newNodeName: " + node._name);
        this._edges[nameOrIndex] = new InspectorTest.HeapEdge(node, type, nameOrIndex);
    },

    _setBuilder: function(builder)
    {
        if (this._builder)
            throw new Error("node reusing is prohibited");

        this._builder = builder;
        this._ordinal = this._builder._registerNode(this);
    },

    _serialize: function(rawSnapshot)
    {
        rawSnapshot.nodes.push(this._builder.lookupNodeType(this._type));
        rawSnapshot.nodes.push(this._builder.lookupOrAddString(this._name));
        // JS engine snapshot impementation generates monotonicaly increasing odd id for JS objects,
        // and even ids based on a hash for native DOMObject groups.
        rawSnapshot.nodes.push(this._id || this._ordinal * 2 + 1);
        rawSnapshot.nodes.push(this._selfSize);
        rawSnapshot.nodes.push(0);                               // retained_size
        rawSnapshot.nodes.push(0);                               // dominator
        rawSnapshot.nodes.push(Object.keys(this._edges).length); // edge_count

        for (var i in this._edges)
            this._edges[i]._serialize(rawSnapshot);
    }
}

InspectorTest.HeapEdge = function(targetNode, type, nameOrIndex)
{
    this._targetNode = targetNode;
    this._type = type;
    this._nameOrIndex = nameOrIndex;
}

InspectorTest.HeapEdge.prototype = {
    _serialize: function(rawSnapshot)
    {
        if (!this._targetNode._builder)
            throw new Error("Inconsistent state of node: " + this._name + " no builder assigned");
        var builder = this._targetNode._builder;
        rawSnapshot.edges.push(builder.lookupEdgeType(this._type));
        rawSnapshot.edges.push(typeof this._nameOrIndex === "string" ? builder.lookupOrAddString(this._nameOrIndex) : this._nameOrIndex);
        rawSnapshot.edges.push(this._targetNode._ordinal * builder.nodeFieldsCount); // index
    }
}

InspectorTest.HeapEdge.Type = {
    "context": "context",
    "element": "element",
    "property": "property",
    "internal": "internal",
    "hidden": "hidden",
    "shortcut": "shortcut"
};

InspectorTest.HeapSnapshotBuilder = function()
{
    this._nodes = [];
    this._string2id = {};
    this._strings = [];
    this.nodeFieldsCount = 7;

    this._nodeTypesMap = {};
    this._nodeTypesArray = [];
    for (var nodeType in InspectorTest.HeapNode.Type) {
        this._nodeTypesMap[nodeType] = this._nodeTypesArray.length
        this._nodeTypesArray.push(nodeType);
    }

    this._edgeTypesMap = {};
    this._edgeTypesArray = [];
    for (var edgeType in InspectorTest.HeapEdge.Type) {
        this._edgeTypesMap[edgeType] = this._edgeTypesArray.length
        this._edgeTypesArray.push(edgeType);
    }

    this.rootNode = new InspectorTest.HeapNode("root", 0, "object");
    this.rootNode._setBuilder(this);
}

InspectorTest.HeapSnapshotBuilder.prototype = {
    generateSnapshot: function()
    {
        var rawSnapshot = {
            "snapshot": {
                "meta": {
                    "node_fields": ["type","name","id","self_size","retained_size","dominator","edge_count"],
                    "node_types": [
                        this._nodeTypesArray,
                        "string",
                        "number",
                        "number",
                        "number",
                        "number",
                        "number"
                    ],
                    "edge_fields": ["type","name_or_index","to_node"],
                    "edge_types": [
                        this._edgeTypesArray,
                        "string_or_number",
                        "node"
                    ]
                }
            },
            "nodes": [],
            "edges":[],
            "strings": [],
            maxJSObjectId: this._nodes.length * 2 + 1
        };

        for (var i = 0; i < this._nodes.length; ++i)
            this._nodes[i]._serialize(rawSnapshot);

        rawSnapshot.strings = this._strings.slice();

        var meta = rawSnapshot.snapshot.meta;
        rawSnapshot.snapshot.edge_count = rawSnapshot.edges.length / meta.edge_fields.length;
        rawSnapshot.snapshot.node_count = rawSnapshot.nodes.length / meta.node_fields.length;

        return rawSnapshot;
    },

    _registerNode: function(node)
    {
        this._nodes.push(node);
        return this._nodes.length - 1;
    },

    lookupNodeType: function(typeName)
    {
        if (typeName === undefined)
            throw new Error("wrong node type: " + typeName);
        if (!typeName in this._nodeTypesMap)
            throw new Error("wrong node type name: " + typeName);
        return this._nodeTypesMap[typeName];
    },

    lookupEdgeType: function(typeName)
    {
        if (!typeName in this._edgeTypesMap)
            throw new Error("wrong edge type name: " + typeName);
        return this._edgeTypesMap[typeName];
    },

    lookupOrAddString: function(string)
    {
        if (string in this._string2id)
            return this._string2id[string];
        this._string2id[string] = this._strings.length;
        this._strings.push(string);
        return this._strings.length - 1;
    }
}

InspectorTest.createHeapSnapshot = function(instanceCount, firstId)
{
    // Mocking results of running the following code:
    // 
    // function A() { this.a = this; }
    // function B(x) { this.a = new A(x); }
    // for (var i = 0; i < instanceCount; ++i) new B();
    // 
    // Set A and B object sizes to pseudo random numbers. It is used in sorting tests.

    var seed = 881669;
    function pseudoRandom(limit) {
        seed = ((seed * 355109 + 153763) >> 2) & 0xffff;
        return seed % limit;
    }

    var builder = new InspectorTest.HeapSnapshotBuilder();
    var rootNode = builder.rootNode;

    var gcRootsNode = new InspectorTest.HeapNode("(GC roots)");
    rootNode.linkNode(gcRootsNode, InspectorTest.HeapEdge.Type.element);

    var windowNode = new InspectorTest.HeapNode("Window", 20);
    rootNode.linkNode(windowNode, InspectorTest.HeapEdge.Type.shortcut);
    gcRootsNode.linkNode(windowNode, InspectorTest.HeapEdge.Type.element);

    for (var i = 0; i < instanceCount; ++i) {
        var sizeOfB = pseudoRandom(20) + 1;
        var nodeB = new InspectorTest.HeapNode("B", sizeOfB, undefined, firstId++);
        windowNode.linkNode(nodeB, InspectorTest.HeapEdge.Type.element);
        var sizeOfA = pseudoRandom(50) + 1;
        var nodeA = new InspectorTest.HeapNode("A", sizeOfA, undefined, firstId++);
        nodeB.linkNode(nodeA, InspectorTest.HeapEdge.Type.property, "a");
        nodeA.linkNode(nodeA, InspectorTest.HeapEdge.Type.property, "a");
    }
    return builder.generateSnapshot();
}

InspectorTest.expandRow = function(row, callback)
{
    callback = InspectorTest.safeWrap(callback);
    function populateComplete()
    {
        row.removeEventListener("populate complete", populateComplete, this);
        function callCallback()
        {
            callback(row);
        }
        setTimeout(callCallback, 0);
    }
    row.addEventListener("populate complete", populateComplete, this);
    (function expand()
    {
        if (row.hasChildren)
            row.expand();
        else
            setTimeout(expand, 0);
    })();
};

InspectorTest.findAndExpandGCRoots = function(callback)
{
    InspectorTest.findAndExpandRow("(GC roots)", callback);
};

InspectorTest.findAndExpandWindow = function(callback)
{
    InspectorTest.findAndExpandRow("Window", callback);
};

InspectorTest.findAndExpandRow = function(name, callback)
{
    callback = InspectorTest.safeWrap(callback);
    function propertyMatcher(data)
    {
        return data.value === name;
    }
    var row = InspectorTest.findRow("object", propertyMatcher);
    InspectorTest.assertEquals(true, !!row, '"' + name + '" row');
    InspectorTest.expandRow(row, callback);
};

InspectorTest.findButtonsNode = function(row, startNode)
{
    var result = 0;
    for (var node = startNode || row.children[0]; node; node = node.traverseNextNode(true, row, true)) {
        if (!node.selectable && node.showNext)
            return node;
    }
    return null;
};

InspectorTest.findRow = function(columnIdentifier, matcher, parent)
{
    parent = parent || this._currentGrid().rootNode();
    if (typeof matcher !== "function") {
        var value = matcher;
        matcher = function(x) { return x === value; };
    }
    for (var node = parent.children[0]; node; node = node.traverseNextNode(true, parent, true)) {
        if (matcher(node.data[columnIdentifier]))
            return node;
    }
    return null;
};

InspectorTest.findRow2 = function(matcher, parent)
{
    parent = parent || this._currentGrid().rootNode();
    for (var node = parent.children[0]; node; node = node.traverseNextNode(true, parent, true)) {
        if (matcher(node.data))
            return node;
    }
    return null;
};

InspectorTest.switchToView = function(title, callback)
{
    callback = InspectorTest.safeWrap(callback);
    var view = WebInspector.panels.profiles.visibleView;
    view.changeView(title, callback);
};

InspectorTest.takeAndOpenSnapshot = function(generator, callback)
{
    callback = InspectorTest.safeWrap(callback);
    var uid = InspectorTest._nextUid++;
    var snapshot = generator();
    var profileType = WebInspector.panels.profiles.getProfileType(WebInspector.HeapSnapshotProfileType.TypeId);
    var profile = profileType.createProfile({
        title: WebInspector.ProfilesPanelDescriptor.UserInitiatedProfileName + "." + uid,
        uid: uid,
        maxJSObjectId: snapshot.maxJSObjectId
    });
    delete snapshot.maxJSObjectId;
    function pushGeneratedSnapshot(uid)
    {
        snapshot.snapshot.typeId = "HEAP";
        snapshot.snapshot.title = profile.title;
        snapshot.snapshot.uid = profile.uid;
        profileType.addHeapSnapshotChunk(uid, JSON.stringify(snapshot));
        profileType.finishHeapSnapshot(uid);
    }
    InspectorTest.override(HeapProfilerAgent, "getHeapSnapshot", pushGeneratedSnapshot);
    InspectorTest._takeAndOpenSnapshotCallback = callback;
    profileType.addProfile(profile);
    WebInspector.panels.profiles._showProfile(profile);
};

InspectorTest.viewColumns = function()
{
    return InspectorTest._currentGrid()._columnsArray;
};

InspectorTest._currentProfileView = function()
{
    return WebInspector.panels.profiles.visibleView;
};

InspectorTest._currentGrid = function()
{
    return this._currentProfileView().dataGrid;
};

InspectorTest._snapshotViewShown = function()
{
    if (InspectorTest._takeAndOpenSnapshotCallback) {
        var callback = InspectorTest._takeAndOpenSnapshotCallback;
        InspectorTest._takeAndOpenSnapshotCallback = null;
        var dataGrid = this.dataGrid;
        function sortingComplete()
        {
            dataGrid.removeEventListener("sorting complete", sortingComplete, null);
            callback();
        }
        dataGrid.addEventListener("sorting complete", sortingComplete, null);
    }
};

};
