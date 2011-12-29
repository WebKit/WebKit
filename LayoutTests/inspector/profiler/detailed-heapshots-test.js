var initialize_DetailedHeapshotTest = function() {

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
        InspectorTest.addSniffer(WebInspector.DetailedHeapshotView.prototype, "_updatePercentButton", InspectorTest._snapshotViewShown, true);

        detailedHeapProfilesEnabled();
    }

    function detailedHeapProfilesEnabled()
    {
        // Reduce the number of populated nodes to speed up testing.
        WebInspector.HeapSnapshotContainmentDataGrid.prototype._defaultPopulateCount = 10;
        WebInspector.HeapSnapshotConstructorsDataGrid.prototype._defaultPopulateCount = 10;
        WebInspector.HeapSnapshotDiffDataGrid.prototype._defaultPopulateCount = 5;
        WebInspector.HeapSnapshotDominatorsDataGrid.prototype._defaultPopulateCount = 3;
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

InspectorTest.runDetailedHeapshotTestSuite = function(testSuite)
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
        if (!data.value)
            InspectorTest.addResult("No value field in " + JSON.stringify(data));
        var indexOfAt = data.value.indexOf("@");
        if (indexOfAt === -1)
            InspectorTest.addResult("Can't find @ in " + data.value);
        return parseInt(data.value.substring(indexOfAt + 1), 10);
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
    var result = [];
    var parent = row || this._currentGrid();
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
        return data.value === "(GC roots) @3";
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
    parent = parent || this._currentGrid();
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
    parent = parent || this._currentGrid();
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
    var profile = { typeId: WebInspector.DetailedHeapshotProfileType.TypeId, uid: uid, title: UserInitiatedProfileName + "." + uid };
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
