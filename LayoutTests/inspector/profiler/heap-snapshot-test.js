var initialize_HeapSnapshotTest = function() {

InspectorTest.createHeapSnapshotMockObject = function()
{
    return {
        _rootNodeIndex: 0,
        _nodeTypeOffset: 0,
        _nodeNameOffset: 1,
        _edgesCountOffset: 2,
        _firstEdgeIndexOffset: 2,
        _firstEdgeOffset: 3,
        _nodeFieldCount: 3,
        _edgeFieldsCount: 3,
        _edgeTypeOffset: 0,
        _edgeNameOffset: 1,
        _edgeToNodeOffset: 2,
        _nodeTypes: ["hidden", "object"],
        _edgeTypes: ["element", "property"],
        _edgeShortcutType: -1,
        _edgeHiddenType: -1,
        _edgeElementType: 0,
        // Represents the following graph:
        //   (numbers in parentheses indicate node offset)
        // 
        //         A (3) --ac- C (9) -ce- E(15)
        //       a/|         /
        //  "" (0) 1      bc
        //       b\v    /
        //         B (6) -bd- D (12)
        //
        _onlyNodes: [
            0, 0, 0,
            1, 1, 6,
            1, 2, 12,
            1, 3, 18,
            1, 4, 21,
            1, 5, 21],
        _containmentEdges: [
            1,  6, 3, 1,  7, 6,
            0,  1, 6, 1,  8, 9,
            1,  9, 9, 1, 10, 12,
            1, 11, 15],
        _strings: ["", "A", "B", "C", "D", "E", "a", "b", "ac", "bc", "bd", "ce"]
    };
};

InspectorTest.createHeapSnapshotMockRaw = function()
{
    return {
        snapshot: {},
        nodes: [
            { fields: ["type", "name", "id", "self_size", "retained_size", "dominator", "children_count", "children"],
              types: [["hidden", "object"], "", "", "", "", "", "", { fields: ["type", "name_or_index", "to_node"], types: [["element", "property"], "", ""] }] },
            // Effectively the same graph as in createHeapSnapshotMockObject,
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
            0, 0, 1, 0, 20,  1, 2, 1,  6, 14, 1,  7, 27,
            1, 1, 2, 2,  2,  1, 2, 0,  1, 27, 1,  8, 40,
            1, 2, 3, 3,  8,  1, 2, 1,  9, 40, 1, 10, 50,
            1, 3, 4, 4, 10,  1, 1, 1, 11, 57,
            1, 4, 5, 5,  5, 27, 0,
            1, 5, 6, 6,  6, 40, 0],
        strings: ["", "A", "B", "C", "D", "E", "a", "b", "ac", "bc", "bd", "ce"]
    };
};

InspectorTest.createHeapSnapshotSplitNodesEdgesMockRaw = function()
{
    // Return the same snapshot as above but having the nodes and edges
    // separated.
    return {
        snapshot: {
            meta: {
                node_fields: ["type", "name", "id", "self_size", "retained_size", "dominator", "edges_index"],
                node_types: [["hidden", "object"], "", "", "", "", "", ""],
                edge_fields: ["type", "name_or_index", "to_node"],
                edge_types: [["element", "property"], "", ""]
            },
            node_count: 6,
            edge_count: 7},
        nodes: [
            0, 0, 1, 0, 20,  0,  0,
            1, 1, 2, 2,  2,  0,  6,
            1, 2, 3, 3,  8,  0, 12,
            1, 3, 4, 4, 10,  0, 18,
            1, 4, 5, 5,  5, 14, 21,
            1, 5, 6, 6,  6, 21, 21],
        edges: [
            1,  6,  7, 1,  7, 14,
            0,  1, 14, 1,  8, 21,
            1,  9, 21, 1, 10, 28,
            1, 11, 35],
        strings: ["", "A", "B", "C", "D", "E", "a", "b", "ac", "bc", "bd", "ce"]
    };
};

InspectorTest._postprocessHeapSnapshotMock = function(mock)
{
    mock.snapshot.meta = mock.nodes[0];
    mock.nodes[0] = 0;
    var tempNodes = new Int32Array(1000);
    tempNodes.set(mock.nodes);
    mock.nodes = tempNodes.subarray(0, mock.nodes.length);
    return mock;
};

InspectorTest.createHeapSnapshotMock = function()
{
    return InspectorTest._postprocessHeapSnapshotMock(InspectorTest.createHeapSnapshotMockRaw());
};

InspectorTest.createHeapSnapshotMockWithDOM = function()
{
    return InspectorTest._postprocessHeapSnapshotMock({
        snapshot: {},
        nodes: [
            { fields: ["type", "name", "id", "children_count", "children"],
              types: [["hidden", "object"], "", "", "", { fields: ["type", "name_or_index", "to_node"], types: [["element", "hidden", "internal"], "", ""] }] },
            // A tree with Window objects.
            //
            //    |----->Window--->A
            //    |                \
            //    |----->Window--->B--->C
            //    |        |     \
            //  (root)   hidden   --->D--internal / "native"-->N
            //    |         \         |
            //    |----->E   H     internal
            //    |                   v
            //    |----->F--->G------>M
            //
            /* (root) */    0,  0,  1, 4, 0,  1, 17, 0, 2, 27, 0, 3, 40, 0, 4, 44,
            /* Window */    1, 11,  2, 2, 0,  1, 51, 0, 2, 55,
            /* Window */    1, 11,  3, 3, 0,  1, 55, 0, 2, 62, 1, 3, 72,
            /* E */         1,  5,  4, 0,
            /* F */         1,  6,  5, 1, 0,  1, 76,
            /* A */         1,  1,  6, 0,
            /* B */         1,  2,  7, 1, 0,  1, 80,
            /* D */         1,  4,  8, 2, 2, 12, 84, 2, 1, 88,
            /* H */         1,  8,  9, 0,
            /* G */         1,  7, 10, 0,
            /* C */         1,  3, 11, 0,
            /* N */         1, 10, 12, 0,
            /* M */         1,  9, 13, 0
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
        // We mock out ProfilerAgent -- as DRT runs in single-process mode, Inspector
        // and test share the same heap. Taking a snapshot takes too long for a test,
        // so we provide synthetic snapshots.
        InspectorTest._panelReset = InspectorTest.override(WebInspector.panels.profiles, "_reset", function(){}, true);
        InspectorTest.addSniffer(WebInspector.HeapSnapshotView.prototype, "_loadProfile", InspectorTest._snapshotViewShown, true);

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
        if (size.charAt(0) === ">")
            size = size.substring(2);
        var amount = parseFloat(size, 10);
        var multiplier = {
            "KB": 1024,
            "MB": 1024 * 1024
        }[size.substring(size.length - 2)];
        return multiplier ? amount * multiplier : amount;
    }
    function extractName(data)
    {
        data = JSON.parse(data);
        if (!data.name)
            InspectorTest.addResult("No name field in " + JSON.stringify(data));
        return parseInt(data.name, 10);
    }
    function extractId(data)
    {
        data = JSON.parse(data);
        if (!data.nodeId)
            InspectorTest.addResult("No nodeId field in " + JSON.stringify(data));
        return parseInt(data.nodeId, 10);
    }
    var comparator = {
        text: simpleComparator,
        number: function (a, b) { return simpleComparator(parseInt(a, 10), parseInt(b, 10)); },
        size: function (a, b) { return simpleComparator(parseSize(a), parseSize(b)); },
        name: function (a, b) { return simpleComparator(extractName(a), extractName(b)); },
        id: function (a, b) { return simpleComparator(extractId(a), extractId(b)); }
    }[sortType];
    var acceptableComparisonResult = {
        ascending: -1,
        descending: 1
    }[sortOrder];

    if (!comparator) {
        InspectorTest.addResult("Invalid sort type: " + sortType);
        return;
    }
    if (!acceptableComparisonResult) {
        InspectorTest.addResult("Invalid sort order: " + sortOrder);
        return;
    }

    for (var i = 0; i < contents.length - 1; ++i) {
        var result = comparator(contents[i], contents[i + 1]);
        if (result !== 0 && result !== acceptableComparisonResult)
            InspectorTest.addResult("Elements " + i + " and " + (i + 1) + " are out of order: " + contents[i] + " " + contents[i + 1] + " (" + sortOrder + ")");
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

InspectorTest.createHeapSnapshot = function(instanceCount, firstId)
{
    // Mocking results of running the following code:
    // 
    // function A() { this.a = this; }
    // function B(x) { this.a = new A(x); }
    // for (var i = 0; i < instanceCount; ++i) new B();
    // 
    // Instances of A have 12 bytes size, instances of B has 16 bytes size.
    var sizeOfA = 12;
    var sizeOfB = 16;  

    function generateNodes()
    {
        var nodes = [null];
        // Push the 'meta-root' node.
        nodes.push(0, 0, 1, 0, (sizeOfA + sizeOfB) * instanceCount, 1, 1, 4, 1, null);
        // Push instances of A and B.
        var indexesOfB = [];
        var nextId = firstId || 5;
        for (var i = 0; i < instanceCount; ++i) {
            var indexOfA = nodes.length;
            nodes.push(3, 1, nextId++, sizeOfA, sizeOfA, null, 1, 2, 3, indexOfA);
            var indexOfB = nodes.length;
            // Set dominator of A.
            nodes[indexOfA + 5] = indexOfB;
            nodes.push(3, 2, nextId++, sizeOfB, sizeOfB + sizeOfA, null, 1, 2, 3, indexOfA);
            indexesOfB.push(indexOfB);
        }
        var indexOfGCRoots = nodes.length;
        nodes.push(3, 4, 3, 0, (sizeOfA + sizeOfB) * instanceCount, 1, instanceCount);
        // Set dominator of B.
        for (var i = 0; i < instanceCount; ++i) {
            nodes[indexesOfB[i] + 5] = indexOfGCRoots;
        }
        // Set (GC roots) as child of meta-root.
        nodes[10] = indexOfGCRoots;
        // Push instances of B as children of GC roots.
        for (var i = 0; i < instanceCount; ++i) {
            nodes.push(1, i + 1, indexesOfB[i]);
        }
        return nodes;
    }

    var result = {
        "snapshot": {},
        "nodes": generateNodes(),
        "strings": ["", "A", "B", "a", "(GC roots)"]
    };
    result.nodes[0] = {
        "fields":["type","name","id","self_size","retained_size","dominator","children_count","children"],
        "types":[["hidden","array","string","object","code","closure","regexp","number","native"],"string","number","number","number","number","number",{
            "fields":["type","name_or_index","to_node"],
            "types":[["context","element","property","internal","hidden","shortcut"],"string_or_number","node"]}]};
    return result;
};

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
    callback = InspectorTest.safeWrap(callback);
    function propertyMatcher(data)
    {
        return data.value === "(GC roots)";
    }
    var gcRoots = InspectorTest.findRow("object", propertyMatcher);
    InspectorTest.assertEquals(true, !!gcRoots, "GC roots row");
    InspectorTest.expandRow(gcRoots, callback);
}

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
    var profile = { typeId: WebInspector.HeapSnapshotProfileType.TypeId, uid: uid, title: UserInitiatedProfileName + "." + uid };
    function pushGeneratedSnapshot(typeId, uid)
    {
        var snapshot = generator();
        snapshot.snapshot.typeId = profile.typeId;
        snapshot.snapshot.title = profile.title;
        snapshot.snapshot.uid = profile.uid;
        WebInspector.panels.profiles._addHeapSnapshotChunk(uid, JSON.stringify(snapshot));
        WebInspector.panels.profiles._finishHeapSnapshot(uid);
    }
    InspectorTest.override(ProfilerAgent, "getProfile", pushGeneratedSnapshot);
    InspectorTest._takeAndOpenSnapshotCallback = callback;
    var profileType = WebInspector.panels.profiles.getProfileType(profile.typeId);
    profile = profileType.createProfile(profile);
    WebInspector.panels.profiles.addProfileHeader(profile);
    WebInspector.panels.profiles.showProfile(profile);
};

InspectorTest.viewColumns = function()
{
    return InspectorTest._currentGrid()._columnsArray;
};

InspectorTest._currentGrid = function()
{
    return WebInspector.panels.profiles.visibleView.dataGrid;
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
