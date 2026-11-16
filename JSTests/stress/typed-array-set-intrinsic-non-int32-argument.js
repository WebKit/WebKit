//@ skip if not $jitTests
//@ $skipModes << :lockdown

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

let functionCount = 0;

function makeSet(body) {
    const set = new Function("target", "source", "values", "i", `${body} // ${functionCount++}`);
    noInline(set);
    return set;
}

function test(body, values, expectedOffset) {
    const set = makeSet(body);
    const source = new Uint8Array(3);
    const target = new Uint8Array(16);
    for (let i = 0; i < testLoopCount * 10; i++) {
        source[0] = i;
        source[2] = i + 2;
        set(target, source, values, i);
        const offset = expectedOffset(i);
        shouldBe(target[offset], i & 0xff);
        shouldBe(target[offset + 2], (i + 2) & 0xff);
    }
    shouldBe(numberOfDFGCompiles(set) <= 3, true);
}

function testNumberSource(body, values) {
    const set = makeSet(body);
    const target = new Uint8Array(16);
    for (let i = 0; i < testLoopCount * 10; i++) {
        set(target, null, values, i);
        shouldBe(target[i & 15], 0);
    }
    shouldBe(numberOfDFGCompiles(set) <= 3, true);
}

const doubles = new Float64Array([0, 1, 2, 3, 4, 5, 6, 7]);

test("target.set(source, values[i & 7]);", doubles, i => i & 7);
test("target.set(source, (i & 7) * 0.5);", null, i => (i & 7) >> 1);
test("for (let j = 0.5; j < 2; j++) target.set(source, j);", null, i => 1);
test("target.set(source, undefined);", null, i => 0);
test("target.set(source, null);", null, i => 0);
test("target.set(source, 1.5);", null, i => 1);
test("target.set(source, '2');", null, i => 2);
testNumberSource("target.set(values[i & 7], 0);", doubles);
testNumberSource("target.set(42, 0);", null);
