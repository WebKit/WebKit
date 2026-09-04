function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}`);
}

function countSimpleObjects() {
    let snapshot = generateHeapSnapshot();
    let count = 0;
    for (let i = 0; i < snapshot.nodes.length; i += 4) {
        if (snapshot.nodeClassNames[snapshot.nodes[i + 2]] === "SimpleObject")
            ++count;
    }
    return count;
}

function* shrinking() {
    let a = 1;
    let b = 2;
    let object = new $vm.SimpleObject;
    yield 0;
    shouldBe(b, 2);
    shouldBe(typeof object, "object");
    object = null;
    yield 1;
    return a + (object === null ? 10 : 20);
}

let iterator = shrinking();
shouldBe(iterator.next().value, 0);
fullGC();
shouldBe(countSimpleObjects(), 1);
shouldBe(iterator.next().value, 1);
fullGC();
shouldBe(countSimpleObjects(), 0);
shouldBe(iterator.next().value, 11);
