function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

let counter = 0;
let obj = { valueOf() { counter++; return 1; } };

function test(x) {
    return Math.pow(x);
}
noInline(test);

for (let i = 0; i < testLoopCount; i++)
    shouldBe(Number.isNaN(test(obj)), true);

shouldBe(counter, testLoopCount);

for (let i = 0; i < testLoopCount; i++)
    shouldBe(Number.isNaN(test(42)), true);

function testThrow(x) {
    return Math.pow(x);
}
noInline(testThrow);

for (let i = 0; i < testLoopCount; i++)
    shouldBe(Number.isNaN(testThrow(1.5)), true);

let throwObj = { valueOf() { throw new Error("ok"); } };
let caught = false;
try {
    testThrow(throwObj);
} catch (e) {
    caught = true;
    shouldBe(e.message, "ok");
}
shouldBe(caught, true);
