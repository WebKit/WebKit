load("./driver/driver.js");

let simpleObject = new SimpleObject;
setHiddenValue(simpleObject, "hiddenValue"); // Internal
simpleObject.propertyName1 = "propertyValue1"; // Property
simpleObject["propertyName2"] = "propertyValue2"; // Property
simpleObject[100] = "indexedValue"; // Index
simpleObject[0xffffffff + 100] = "largeIndexValueBecomingProperty"; // Property
simpleObject.point = {x:"x1", y:"y1"}; // Property => object with 2 inline properties.

// ----------

function excludeStructure(edges) {
    return edges.filter((x) => x.to.className !== "Structure");
}

let snapshot = createHeapSnapshot();

// Internal, Property, and Index edges on an Object.
let nodes = snapshot.nodesWithClassName("SimpleObject");
assert(nodes.length === 1, "Snapshot should contain 1 'SimpleObject' instance");
let simpleObjectNode = nodes[0];
let edges = excludeStructure(simpleObjectNode.outgoingEdges);
let pointNode = null;

let seenHiddenValue = false;
let seenPropertyName1 = false;
let seenPropertyName2 = false;
let seenIndex100 = false;
let seenLargeIndex = false;
let seenObjectWithInlineStorage = false;
let largeIndexName = (0xffffffff + 100).toString();

for (let edge of edges) {
    switch (edge.type) {
    case "Internal":
        assert(!seenHiddenValue);
        seenHiddenValue = true;
        break;
    case "Property":
        if (edge.data === "propertyName1")
            seenPropertyName1 = true;
        else if (edge.data === "propertyName2")
            seenPropertyName2 = true;
        else if (edge.data === largeIndexName)
            seenLargeIndex = true;
        else if (edge.data === "point") {
            seenPoint = true;
            pointNode = edge.to;
        } else
            assert(false, "Unexpected property name");
        break;
    case "Index":
        if (edge.data === 100)
            seenIndex100 = true;
        break;
    case "Variable":
        assert(false, "Should not see a variable edge for SimpleObject instance");
        break;
    default:
        assert(false, "Unexpected edge type");
        break;
    }
}

assert(seenHiddenValue, "Should see Internal edge for hidden value");
assert(seenPropertyName1, "Should see Property edge for propertyName1");
assert(seenPropertyName2, "Should see Property edge for propertyName2");
assert(seenIndex100, "Should see Index edge for index 100");
assert(seenLargeIndex, "Should see Property edge for index " + largeIndexName);


// Property on an Object's inline storage.
let pointEdges = excludeStructure(pointNode.outgoingEdges);

let seenPropertyX = false;
let seenPropertyY = false;

for (let edge of pointEdges) {
    switch (edge.type) {
    case "Property":
        if (edge.data === "x")
            seenPropertyX = true;
        else if (edge.data === "y")
            seenPropertyY = true;
        else
            assert(false, "Unexpected property name");
        break;
    case "Index":
    case "Variable":
    case "Internal":
        assert(false, "Unexpected edge type");
        break;
    }
}

assert(seenPropertyX, "Should see Property edge for x");
assert(seenPropertyY, "Should see Property edge for y");
