function assert(b, ...m) {
    if (!b)
        throw new Error("Bad: ", ...m);
}
noInline(assert);

function shallowEq(a, b) {
    assert(a.length === b.length, a, b);
    for (let i = 0; i < a.length; i++)
        assert(a[i] === b[i], a, b);
}
noInline(shallowEq);

function runConcat(a, b) {
    return a.concat(b);
}
noInline(runConcat);

function makeContiguous() {
    let a = [1, "x"];
    return a;
}
noInline(makeContiguous);

function makeDouble() {
    let a = [1.5, 2.5];
    return a;
}
noInline(makeDouble);

function testContiguousPlusDouble() {
    let a = makeContiguous();
    let b = makeDouble();
    let r = runConcat(a, b);
    shallowEq(r, [1, "x", 1.5, 2.5]);
}

function testDoublePlusContiguous() {
    let a = makeDouble();
    let b = makeContiguous();
    let r = runConcat(a, b);
    shallowEq(r, [1.5, 2.5, 1, "x"]);
}

function testCoWContiguousPlusDouble() {
    let a = [1, "x"];
    let b = [1.5, 2.5];
    let r = runConcat(a, b);
    shallowEq(r, [1, "x", 1.5, 2.5]);
}

function testCoWDoublePlusContiguous() {
    let a = [1.5, 2.5];
    let b = [1, "x"];
    let r = runConcat(a, b);
    shallowEq(r, [1.5, 2.5, 1, "x"]);
}

function testEmptyContiguousPlusDouble() {
    let a = [];
    let b = [1.5, 2.5];
    shallowEq(runConcat(a, b), [1.5, 2.5]);
}

for (let i = 0; i < testLoopCount; i++) {
    testContiguousPlusDouble();
    testDoublePlusContiguous();
    testCoWContiguousPlusDouble();
    testCoWDoublePlusContiguous();
    testEmptyContiguousPlusDouble();
}
