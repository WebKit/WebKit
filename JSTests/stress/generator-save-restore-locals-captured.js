function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}, expected ${String(expected)}`);
}

function* captured() {
    let counter = 0;
    let local = 100;
    const bump = () => ++counter;
    yield bump();
    local += counter;
    yield bump() + local;
    return [counter, local];
}

for (let i = 0; i < testLoopCount; ++i) {
    let iterator = captured();
    shouldBe(iterator.next().value, 1);
    shouldBe(iterator.next().value, 103);
    shouldBe(JSON.stringify(iterator.next().value), "[2,101]");
}

function* manyCaptured() {
    let c0 = 0, c1 = 1, c2 = 2, c3 = 3, c4 = 4, c5 = 5, c6 = 6, c7 = 7, c8 = 8, c9 = 9;
    const read = () => c0 + c1 + c2 + c3 + c4 + c5 + c6 + c7 + c8 + c9;
    const write = () => { c0 += 10; c9 += 10; };
    let local0 = "a", local1 = "b", local2 = "c";
    yield read();
    write();
    local0 += local1;
    yield read();
    local2 += local0;
    yield read();
    return local0 + local1 + local2;
}

for (let i = 0; i < testLoopCount; ++i) {
    let iterator = manyCaptured();
    shouldBe(iterator.next().value, 45);
    shouldBe(iterator.next().value, 65);
    shouldBe(iterator.next().value, 65);
    shouldBe(iterator.next().value, "abbcab");
}

function* outer() {
    let shared = 1;
    let local = 10;
    function* inner() {
        let innerLocal = 100;
        shared += 1;
        yield innerLocal + shared;
        innerLocal += shared;
        yield innerLocal;
    }
    let iterator = inner();
    yield iterator.next().value + local;
    local += shared;
    yield iterator.next().value + local;
    return [shared, local];
}

for (let i = 0; i < testLoopCount; ++i) {
    let iterator = outer();
    shouldBe(iterator.next().value, 112);
    shouldBe(iterator.next().value, 114);
    shouldBe(JSON.stringify(iterator.next().value), "[2,12]");
}
