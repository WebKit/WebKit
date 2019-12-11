var SimpleObject = $vm.SimpleObject;

load("./driver/driver.js");

function hasDifferentSizeNodes(nodes) {
    let seenSize = nodes[0].size;
    for (let node of nodes) {
        if (node.size !== seenSize)
            return true;
    }
    return false;
}

function hasAllInternalNodes(nodes) {
    for (let node of nodes) {
        if (!node.internal)
            return false;
    }
    return true;
}

function sorted(nodes) {
    return nodes.sort((a, b) => a.id - b.id);
}

let simpleObject1NodeId;
let simpleObject2NodeId;

(function() {
    let snapshot = createCheapHeapSnapshot();
    assert(snapshot.nodesWithClassName("global").length === 1, "Snapshot should contain a single 'global' node");
    assert(snapshot.nodesWithClassName("Structure").length > 0, "Snapshot should contain 'Structure' nodes");
    assert(snapshot.nodesWithClassName("ThisClassNameDoesNotExist").length === 0, "Snapshot should not contain 'ThisClassNameDoesNotExist' nodes");

    let strings = snapshot.nodesWithClassName("string");
    assert(strings.length > 0, "Snapshot should contain 'string' nodes");
    assert(hasDifferentSizeNodes(strings), "'string' nodes should have different sizes");

    let nativeExecutables = snapshot.nodesWithClassName("NativeExecutable");
    assert(nativeExecutables.length > 0, "Snapshot should contain 'NativeExecutable' nodes");
    assert(!hasDifferentSizeNodes(nativeExecutables), "'NativeExecutable' nodes should all be the same size");
    assert(hasAllInternalNodes(nativeExecutables), "'NativeExecutable' nodes should all be internal");

    assert(snapshot.nodesWithClassName("SimpleObject").length === 0, "Snapshot should not contain a 'SimpleObject' instance");
})();

let simpleObject1 = new SimpleObject;

(function() {
    let snapshot = createCheapHeapSnapshot();
    let nodes = sorted(snapshot.nodesWithClassName("SimpleObject"));
    let [simpleObject1Node] = nodes;
    simpleObject1NodeId = nodes[0].id;
    assert(nodes.length === 1, "Snapshot should contain 1 'SimpleObject' instance");
    assert(simpleObject1Node.outgoingEdges.length === 1, "'simpleObject1' should only reference its structure");
    assert(simpleObject1Node.outgoingEdges[0].to.className === "Structure", "'simpleObject1' should reference a Structure");
})();

let simpleObjectList = [];
for (let i = 0; i < 1234; ++i)
    simpleObjectList.push(new SimpleObject);

(function() {
    let snapshot = createCheapHeapSnapshot();
    let nodes = sorted(snapshot.nodesWithClassName("SimpleObject"));
    simpleObject1NodeId = nodes[0].id;
    simpleObject2NodeId = nodes[1].id;
    assert(nodes.length === 1235, "Snapshot should contain 1235 'SimpleObject' instances");
    assert(nodes[0].id === simpleObject1NodeId, "'simpleObject1' should maintain the same identifier");
    assert(simpleObject1NodeId < simpleObject2NodeId, "node identifiers should always increase across snapshots");
})();

simpleObject1 = null;
simpleObjectList.fill(null);

(function() {
    let snapshot = createCheapHeapSnapshot();
    let nodes = snapshot.nodesWithClassName("SimpleObject");
    assert(nodes.length === 0, "Snapshot should not contain a 'SimpleObject' instance");
})();
