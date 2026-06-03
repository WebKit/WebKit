// Test that polymorphic inline cache stubs work correctly with call/ret dispatch.
// When the IC becomes polymorphic, the slab is rewritten with a call to the compiled stub
// which returns via ret. This tests the basic get/put/in/delete/instanceof paths.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("Expected: " + expected + " but got: " + actual);
}

// -- GetById polymorphic --
function getX(o) { return o.x; }
noInline(getX);

let shapes = [];
for (let i = 0; i < 5; i++) {
    let o = {};
    for (let j = 0; j < i; j++)
        o["p" + j] = j;
    o.x = i * 100;
    shapes.push(o);
}

for (let iter = 0; iter < 1e5; iter++) {
    for (let s of shapes)
        shouldBe(getX(s), s.x);
}

// -- PutById polymorphic --
function putX(o, v) { o.x = v; }
noInline(putX);

for (let iter = 0; iter < 1e5; iter++) {
    for (let s of shapes) {
        putX(s, iter);
        shouldBe(s.x, iter);
    }
}

// -- InById polymorphic --
function hasX(o) { return "x" in o; }
noInline(hasX);

let hasShapes = [{ x: 1 }, { x: 2, y: 1 }, { x: 3, y: 1, z: 1 }, { a: 1 }];
for (let iter = 0; iter < 1e5; iter++) {
    shouldBe(hasX(hasShapes[0]), true);
    shouldBe(hasX(hasShapes[1]), true);
    shouldBe(hasX(hasShapes[2]), true);
    shouldBe(hasX(hasShapes[3]), false);
}

// -- DeleteById polymorphic --
function delX(o) { delete o.x; }
noInline(delX);

for (let iter = 0; iter < 1e5; iter++) {
    let o1 = { x: 1 };
    let o2 = { x: 2, y: 1 };
    let o3 = { x: 3, y: 1, z: 1 };
    delX(o1);
    delX(o2);
    delX(o3);
    shouldBe(o1.x, undefined);
    shouldBe(o2.x, undefined);
    shouldBe(o3.x, undefined);
}

// -- InstanceOf polymorphic --
function isArray(o) { return o instanceof Array; }
noInline(isArray);

for (let iter = 0; iter < 1e5; iter++) {
    shouldBe(isArray([]), true);
    shouldBe(isArray({}), false);
    shouldBe(isArray(""), false);
}
