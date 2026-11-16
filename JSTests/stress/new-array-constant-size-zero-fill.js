//@ requireOptions("--useConcurrentJIT=0")

function shouldBe(a, b) {
    if (a !== b)
        throw new Error("expected " + b + " but got " + a);
}

function shouldBeArrayOfHoles(arr, length) {
    shouldBe(arr.length, length);
    for (var i = 0; i < length; ++i) {
        shouldBe(i in arr, false);
        shouldBe(arr[i], undefined);
    }
}

// Sizes that exercise the unrolled path (<= 16) and the loop fallback (> 16).
function make0()    { return new Array(0); }
function make1()    { return new Array(1); }
function make2()    { return new Array(2); }
function make3()    { return new Array(3); }
function make7()    { return new Array(7); }
function make8()    { return new Array(8); }
function make15()   { return new Array(15); }
function make16()   { return new Array(16); }
function make17()   { return new Array(17); }
function make32()   { return new Array(32); }
function make100()  { return new Array(100); }
function make999()  { return new Array(999); }

var fns = [
    [make0, 0],
    [make1, 1],
    [make2, 2],
    [make3, 3],
    [make7, 7],
    [make8, 8],
    [make15, 15],
    [make16, 16],
    [make17, 17],
    [make32, 32],
    [make100, 100],
    [make999, 999],
];

for (var i = 0; i < testLoopCount; ++i) {
    for (var j = 0; j < fns.length; ++j) {
        var [fn, len] = fns[j];
        shouldBeArrayOfHoles(fn(), len);
    }
}

// Force ArrayWithDouble indexing via the array allocation profile, then
// allocate fresh constant-sized arrays through the same call site to make sure
// the (newly canonical) PNaN bit pattern is still treated as a hole.
function makeProfiled(seed) {
    var a = new Array(8);
    for (var i = 0; i < 8; ++i)
        a[i] = i + seed + 0.5; // doubles
    return a;
}
for (var i = 0; i < testLoopCount; ++i)
    makeProfiled(i);

function freshAfterDoubleProfile(len) {
    return new Array(len);
}
for (var i = 0; i < testLoopCount; ++i) {
    var a = freshAfterDoubleProfile(8);
    shouldBe(a.length, 8);
    for (var k = 0; k < 8; ++k) {
        shouldBe(k in a, false);
        shouldBe(a[k], undefined);
    }
}

// Mutate after creation to confirm storage is writable.
function mutate() {
    var a = new Array(8);
    for (var i = 0; i < 8; ++i)
        a[i] = i * 2;
    return a;
}
for (var i = 0; i < testLoopCount; ++i) {
    var a = mutate();
    shouldBe(a.length, 8);
    for (var k = 0; k < 8; ++k)
        shouldBe(a[k], k * 2);
}
