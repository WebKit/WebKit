load("./driver/driver.js");

function excludeStructure(edges) {
    return edges.filter((x) => x.to.className !== "Structure");
}

let simpleObject1NodeId;
let simpleObject2NodeId;

let simpleObject1 = new SimpleObject;
let simpleObject2 = new SimpleObject;

(function() {
    let snapshot = createCheapHeapSnapshot();
    let nodes = snapshot.nodesWithClassName("SimpleObject");
    assert(nodes.length === 2, "Snapshot should contain 2 'SimpleObject' instances");
    let simpleObject1Node = nodes[0].outgoingEdges.length === 2 ? nodes[0] : nodes[1];
    let simpleObject2Node = nodes[0].outgoingEdges.length === 1 ? nodes[0] : nodes[1];
    assert(simpleObject1Node.outgoingEdges.length === 1, "'simpleObject1' should reference only its structure");
    assert(simpleObject2Node.outgoingEdges.length === 1, "'simpleObject2' should reference only its structure");
})();

setHiddenValue(simpleObject1, simpleObject2);

(function() {
    let snapshot = createCheapHeapSnapshot();
    let nodes = snapshot.nodesWithClassName("SimpleObject");
    assert(nodes.length === 2, "Snapshot should contain 2 'SimpleObject' instances");
    let simpleObject1Node = nodes[0].outgoingEdges.length === 2 ? nodes[0] : nodes[1];
    let simpleObject2Node = nodes[0].outgoingEdges.length === 1 ? nodes[0] : nodes[1];
    assert(simpleObject1Node.outgoingEdges.length === 2, "'simpleObject1' should reference its structure and hidden value");
    assert(simpleObject2Node.outgoingEdges.length === 1, "'simpleObject2' should reference only its structure");
    assert(excludeStructure(simpleObject1Node.outgoingEdges)[0].to.id === simpleObject2Node.id, "'simpleObject1' should reference 'simpleObject2'");
    simpleObject1NodeId = simpleObject1Node.id;
    simpleObject2NodeId = simpleObject2Node.id;
})();

simpleObject2 = null;

(function() {
    let snapshot = createCheapHeapSnapshot();
    let nodes = snapshot.nodesWithClassName("SimpleObject");
    assert(nodes.length === 2, "Snapshot should contain 2 'SimpleObject' instances");
    let simpleObject1Node = nodes[0].id === simpleObject1NodeId ? nodes[0] : nodes[1];
    let simpleObject2Node = nodes[0].id === simpleObject2NodeId ? nodes[0] : nodes[1];
    assert(simpleObject1Node.id === simpleObject1NodeId && simpleObject2Node.id === simpleObject2NodeId, "node identifiers were maintained");
    assert(simpleObject1Node.outgoingEdges.length === 2, "'simpleObject1' should reference its structure and hidden value");
    assert(simpleObject2Node.outgoingEdges.length === 1, "'simpleObject2' should reference only its structure");
    assert(excludeStructure(simpleObject1Node.outgoingEdges)[0].to.id === simpleObject2NodeId, "'simpleObject1' should reference 'simpleObject2'");
})();

simpleObject1 = null;

(function() {
    let snapshot = createCheapHeapSnapshot();
    let nodes = snapshot.nodesWithClassName("SimpleObject");
    assert(nodes.length === 0, "Snapshot should not contain a 'SimpleObject' instance");
})();
