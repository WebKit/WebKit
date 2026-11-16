//@ skip if not $jitTests
//@ $skipModes << :lockdown

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

function putByVal(source, a, b) {
    const result = Object.assign({}, source);
    result[0] = a;
    result[1] = b;
    return result;
}
noInline(putByVal);

function getByVal(target, source) {
    return Object.assign(target, source)[1];
}
noInline(getByVal);

function push(target, source, value) {
    return Object.assign(target, source).push(value);
}
noInline(push);

const source = { type: "string", value: 42 };
for (let i = 0; i < testLoopCount * 3; ++i) {
    const result = putByVal(source, i, true);
    shouldBe(result[0], i);
    shouldBe(result[1], true);
    shouldBe(result.type, "string");
    shouldBe(result.value, 42);
    shouldBe(getByVal([1, i, 3], source), i);
    shouldBe(push([1, 2, 3], source, i), 4);
}
shouldBe(numberOfDFGCompiles(putByVal) <= 1, true);
shouldBe(numberOfDFGCompiles(getByVal) <= 1, true);
shouldBe(numberOfDFGCompiles(push) <= 1, true);
