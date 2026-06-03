// Test that polymorphic IC stubs with getter/setter sub-calls work correctly
// with call/ret dispatch. The stub must preserve the return address across
// the getter/setter JS call (via emitDataICPrepareForCall/RestoreAfterCall).

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("Expected: " + expected + " but got: " + actual);
}

// -- GetById with getter (JS call inside IC stub) --
function getX(o) { return o.x; }
noInline(getX);

let plain1 = { x: 100 };
let plain2 = { x: 200, y: 1 };
let withGetter = { get x() { return 300; } };
let proto = { get x() { return 400; } };
let inheriting = Object.create(proto);

for (let iter = 0; iter < 1e5; iter++) {
    shouldBe(getX(plain1), 100);
    shouldBe(getX(plain2), 200);
    shouldBe(getX(withGetter), 300);
    shouldBe(getX(inheriting), 400);
}

// -- PutById with setter (JS call inside IC stub) --
function setX(o, v) { o.x = v; }
noInline(setX);

let withSetter = { _x: 0, set x(v) { this._x = v; } };
let plainSet = { x: 0 };
let plainSet2 = { x: 0, y: 1 };

for (let iter = 0; iter < 1e5; iter++) {
    setX(withSetter, iter);
    shouldBe(withSetter._x, iter);
    setX(plainSet, iter);
    shouldBe(plainSet.x, iter);
    setX(plainSet2, iter);
    shouldBe(plainSet2.x, iter);
}

// -- Custom getter (C++ call inside IC stub) --
function getByteLen(o) { return o.byteLength; }
noInline(getByteLen);

let ab1 = new ArrayBuffer(10);
let ab2 = new ArrayBuffer(20);
let u8 = new Uint8Array(30);

for (let iter = 0; iter < 1e5; iter++) {
    shouldBe(getByteLen(ab1), 10);
    shouldBe(getByteLen(ab2), 20);
    shouldBe(getByteLen(u8), 30);
}
