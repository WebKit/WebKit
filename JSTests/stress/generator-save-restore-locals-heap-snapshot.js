function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}`);
}

function* generator() {
    let saved = new $vm.SimpleObject;
    yield 1;
    return saved;
}

let iterator = generator();
iterator.next();

let snapshot = generateHeapSnapshot();
shouldBe(snapshot.version, 3);

let classNameForNode = new Map();
for (let i = 0; i < snapshot.nodes.length; i += 4)
    classNameForNode.set(snapshot.nodes[i], snapshot.nodeClassNames[snapshot.nodes[i + 2]]);

let savedNodes = [];
for (let [nodeId, className] of classNameForNode) {
    if (className === "SimpleObject")
        savedNodes.push(nodeId);
}
shouldBe(savedNodes.length, 1);

let retainedByGeneratorFrame = false;
for (let i = 0; i < snapshot.edges.length; i += 4) {
    if (snapshot.edges[i + 1] === savedNodes[0] && classNameForNode.get(snapshot.edges[i]) === "JSLexicalEnvironment")
        retainedByGeneratorFrame = true;
}
shouldBe(retainedByGeneratorFrame, true);
