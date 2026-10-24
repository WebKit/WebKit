function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ', expected ' + expected);
}

function sliceFull(a) { return a.slice(); }
noInline(sliceFull);
function sliceZero(a) { return a.slice(0); }
noInline(sliceZero);
function sliceRange(a, start, end) { return a.slice(start, end); }
noInline(sliceRange);

const sourceInt32 = [1, 2, 3, 4, 5, 6, 7, 8];
const sourceDouble = [1.5, 2.5, 3.5, 4.5];
const sourceContiguous = ["a", "b", "c"];

for (var i = 0; i < 1e5; ++i) {
    var a = sliceFull(sourceInt32);
    shouldBe(a.length, 8);
    shouldBe(a[0], 1);
    shouldBe(a[7], 8);
    shouldBe(sourceInt32.length, 8);

    var b = sliceFull(sourceDouble);
    shouldBe(b.length, 4);
    shouldBe(b[0], 1.5);
    shouldBe(b[3], 4.5);

    var c = sliceFull(sourceContiguous);
    shouldBe(c.length, 3);
    shouldBe(c[0], "a");
    shouldBe(c[2], "c");

    var d = sliceZero(sourceInt32);
    shouldBe(d.length, 8);
    shouldBe(d[0], 1);

    var e = sliceRange(sourceInt32, 2, 5);
    shouldBe(e.length, 3);
    shouldBe(e[0], 3);
    shouldBe(e[2], 5);

    var f = sliceRange(sourceInt32, 0, 8);
    shouldBe(f.length, 8);
    shouldBe(f[0], 1);
}

// Mutation after CoW share must not affect the source.
for (var i = 0; i < 1e5; ++i) {
    var a = sliceFull(sourceInt32);
    a[0] = 100;
    shouldBe(a[0], 100);
    shouldBe(sourceInt32[0], 1);

    var b = sliceFull(sourceInt32);
    b.push(9);
    shouldBe(b.length, 9);
    shouldBe(sourceInt32.length, 8);

    var c = sliceFull(sourceInt32);
    c.pop();
    shouldBe(c.length, 7);
    shouldBe(sourceInt32.length, 8);
}

// Two clones of the same source must be independent.
for (var i = 0; i < 1e5; ++i) {
    var a = sliceFull(sourceInt32);
    var b = sliceFull(sourceInt32);
    a[0] = 100;
    shouldBe(b[0], 1);
    b[1] = 200;
    shouldBe(a[1], 2);
}

// Non-CoW source still works.
function makeNonCow() {
    var a = [1, 2, 3, 4];
    a[0] = 1; // forces ensureWritable -> non-CoW
    return a;
}
for (var i = 0; i < 1e5; ++i) {
    var src = makeNonCow();
    var a = sliceFull(src);
    shouldBe(a.length, 4);
    shouldBe(a[0], 1);
    a[0] = 100;
    shouldBe(src[0], 1);
}

// A separate slice function that only ever sees non-CoW sources, so the
// abstract interpreter can prove the source is non-CoW and the FTL skips
// emitting the CoW share path entirely.
function sliceProvenNonCow(a) { return a.slice(); }
noInline(sliceProvenNonCow);
for (var i = 0; i < 1e5; ++i) {
    var src = makeNonCow();
    var a = sliceProvenNonCow(src);
    shouldBe(a.length, 4);
    shouldBe(a[0], 1);
    a[0] = 100;
    shouldBe(src[0], 1);
}
