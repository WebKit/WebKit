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

function* emptyLiveSet() {
    let object = new $vm.SimpleObject;
    yield 0;
    shouldBe(typeof object, "object");
    object = null;
    yield 1;
    yield 2;
}

function mayThrow(value) {
    if (value)
        throw new Error("thrown");
}
noInline(mayThrow);

function* throughHandler() {
    let object = new $vm.SimpleObject;
    try {
        yield 0;
        shouldBe(typeof object, "object");
        object = null;
        mayThrow(true);
    } catch {
        yield 1;
    }
}

function* throughBackEdge() {
    for (let i = 0; i < 2; ++i) {
        yield i;
        let object = new $vm.SimpleObject;
        yield i + 10;
        shouldBe(typeof object, "object");
    }
}

let iterator = shrinking();
shouldBe(iterator.next().value, 0);
fullGC();
shouldBe(countSimpleObjects(), 1);
shouldBe(iterator.next().value, 1);
fullGC();
shouldBe(countSimpleObjects(), 0);
shouldBe(iterator.next().value, 11);

iterator = emptyLiveSet();
shouldBe(iterator.next().value, 0);
fullGC();
shouldBe(countSimpleObjects(), 1);
shouldBe(iterator.next().value, 1);
fullGC();
shouldBe(countSimpleObjects(), 0);
shouldBe(iterator.next().value, 2);
shouldBe(iterator.next().done, true);

iterator = throughHandler();
shouldBe(iterator.next().value, 0);
fullGC();
shouldBe(countSimpleObjects(), 1);
shouldBe(iterator.next().value, 1);
fullGC();
shouldBe(countSimpleObjects(), 0);
shouldBe(iterator.next().done, true);

iterator = throughBackEdge();
shouldBe(iterator.next().value, 0);
shouldBe(iterator.next().value, 10);
fullGC();
shouldBe(countSimpleObjects(), 1);
shouldBe(iterator.next().value, 1);
fullGC();
shouldBe(countSimpleObjects(), 0);
shouldBe(iterator.next().value, 11);
shouldBe(iterator.next().done, true);
