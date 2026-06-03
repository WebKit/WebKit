// Test that exceptions from getter/setter calls inside polymorphic IC stubs
// are handled correctly with call/ret dispatch. The makeshiftCatchHandler must
// properly unwind the call/ret stack overhead before jumping to the FTL exception handler.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("Expected: " + expected + " but got: " + actual);
}

function shouldThrow(fn, expectedMessage) {
    let threw = false;
    try {
        fn();
    } catch (e) {
        threw = true;
        if (expectedMessage && e.message !== expectedMessage)
            throw new Error("Expected message: " + expectedMessage + " but got: " + e.message);
    }
    if (!threw)
        throw new Error("Expected to throw but didn't");
}

// -- Exception from getter in polymorphic IC --
function getX(o) { return o.x; }
noInline(getX);

let plain = { x: 1 };
let plain2 = { x: 2, y: 1 };
let thrower = { get x() { throw new Error("getter-boom"); } };

for (let iter = 0; iter < 1e5; iter++) {
    shouldBe(getX(plain), 1);
    shouldBe(getX(plain2), 2);
    shouldThrow(() => getX(thrower), "getter-boom");
}

// Verify state is not corrupted after many exceptions
shouldBe(getX(plain), 1);
shouldBe(getX(plain2), 2);

// -- Exception from setter in polymorphic IC --
function setX(o, v) { o.x = v; }
noInline(setX);

let setPlain = { x: 0 };
let setPlain2 = { x: 0, y: 1 };
let setThrower = { set x(v) { throw new Error("setter-boom"); } };

for (let iter = 0; iter < 1e5; iter++) {
    setX(setPlain, iter);
    shouldBe(setPlain.x, iter);
    setX(setPlain2, iter);
    shouldBe(setPlain2.x, iter);
    shouldThrow(() => setX(setThrower, 42), "setter-boom");
}

// -- Exception from proxy in polymorphic IC --
function hasKey(o) { return "x" in o; }
noInline(hasKey);

let hasPlain = { x: 1 };
let hasPlain2 = { x: 2, y: 1 };
let proxyThrower = new Proxy({}, { has() { throw new Error("proxy-boom"); } });

for (let iter = 0; iter < 1e5; iter++) {
    shouldBe(hasKey(hasPlain), true);
    shouldBe(hasKey(hasPlain2), true);
    shouldThrow(() => hasKey(proxyThrower), "proxy-boom");
}
