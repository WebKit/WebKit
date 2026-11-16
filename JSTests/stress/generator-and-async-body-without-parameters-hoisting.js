function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function* generatorNoParams() {
    { function a() { } }
    var b = 1;
    yield typeof a;
    yield b;
}

function* generatorWithParams(a, b) {
    { function a() { } }
    var b;
    yield typeof a;
    yield b;
}

async function asyncNoParams() {
    { function a() { } }
    var b = 1;
    await 1;
    return [typeof a, b];
}

async function asyncWithParams(a, b) {
    { function a() { } }
    var b;
    await 1;
    return [typeof a, b];
}

var asyncArrowNoParams = async () => {
    { function a() { } }
    var b = 1;
    await 1;
    return [typeof a, b];
};

for (var i = 0; i < testLoopCount; ++i) {
    var g = generatorNoParams();
    shouldBe(g.next().value, 'function');
    shouldBe(g.next().value, 1);
    g = generatorWithParams(42, 43);
    shouldBe(g.next().value, 'number');
    shouldBe(g.next().value, 43);
}

var results = [];
asyncNoParams().then(v => results.push(v));
asyncWithParams(42, 43).then(v => results.push(v));
asyncArrowNoParams().then(v => results.push(v));
drainMicrotasks();
shouldBe(JSON.stringify(results), '[["function",1],["number",43],["function",1]]');
