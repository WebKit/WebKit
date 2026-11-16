function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected: " + expected);
}

function add(a, b) { return a + b; }
noInline(add);

function load(o) { return o.x * 2 + o.y; }
noInline(load);

function index(array, i) { return array[i] | 0; }
noInline(index);

function thrower(x) {
    if (x > 5)
        throw new Error("boom" + x);
    return x;
}
noInline(thrower);

function catcher(x) {
    try {
        return thrower(x) + 1;
    } catch (e) {
        return e.message.length;
    }
}
noInline(catcher);

for (let round = 0; round < 4; ++round) {
    for (let i = 0; i < testLoopCount; ++i) {
        shouldBe(add(i, 1), i + 1);
        shouldBe(load({ x: i, y: 1 }), i * 2 + 1);
        shouldBe(index([1, 2, 3], i % 3), i % 3 + 1);
        shouldBe(catcher(i % 5), i % 5 + 1);
    }

    // Each exit fires more than once. The first time goes through the exit generation thunk,
    // and the following times go through the repatched entrance.
    for (let k = 0; k < 3; ++k) {
        shouldBe(add("x", 1), "x1");
        shouldBe(add(1.5, 2.5), 4);
        shouldBe(load({ y: 1, x: 2.5 }), 6);
        shouldBe(load({ x: 1, y: "s" }), "2s");
        shouldBe(index([1, 2, 3], 10), 0);
        shouldBe(index([1.5], 0), 1);
        shouldBe(catcher(7 + k), 5);
    }
}
