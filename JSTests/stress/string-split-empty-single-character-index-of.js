function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected: ${expected}`);
}

function indexOfTest(array, search) {
    return array.indexOf(search);
}
noInline(indexOfTest);

// Dynamically built (non-atom) subject, Latin-1.
let dynamic = "";
for (let i = 0; i < 20; ++i)
    dynamic += String.fromCharCode(97 + i);

for (let i = 0; i < testLoopCount; ++i) {
    let array = dynamic.split("");
    shouldBe(array.length, 20);
    shouldBe(indexOfTest(array, "c"), 2);
    shouldBe(indexOfTest(array, String.fromCharCode(99, 100).substring(0, 1)), 2);
    shouldBe(indexOfTest(array, "z"), -1);
    shouldBe(indexOfTest(array, "Z"), -1);
    shouldBe(indexOfTest(array, "aa"), -1);
}

// Atom (literal) subject: stringSplitCache path.
for (let i = 0; i < testLoopCount; ++i) {
    let array = "greedisgood".split("");
    shouldBe(indexOfTest(array, "s"), 6);
    shouldBe(indexOfTest(array, "z"), -1);
    shouldBe(array.join(""), "greedisgood");
}

// Latin-1 boundary characters: 0x00, 0xFF, and just above (0x100).
let edge = ("x" + String.fromCharCode(0, 255, 256, 65)).substring(1);
let edgeArray = edge.split("");
shouldBe(edgeArray.length, 4);
shouldBe(indexOfTest(edgeArray, String.fromCharCode(0)), 0);
shouldBe(indexOfTest(edgeArray, String.fromCharCode(255)), 1);
shouldBe(indexOfTest(edgeArray, String.fromCharCode(256)), 2);
shouldBe(indexOfTest(edgeArray, "A"), 3);
shouldBe(indexOfTest(edgeArray, String.fromCharCode(1)), -1);

// Non-Latin-1 (16-bit) and mixed 8/16-bit subjects.
let unicode = ("x" + "あいうえお￿").substring(1);
let unicodeArray = unicode.split("");
shouldBe(unicodeArray.length, 6);
shouldBe(indexOfTest(unicodeArray, "う"), 2);
shouldBe(indexOfTest(unicodeArray, "￿"), 5);
shouldBe(indexOfTest(unicodeArray, "か"), -1);
shouldBe(indexOfTest(unicodeArray, "a"), -1);

let mixed = ("x" + "aあb").substring(1);
let mixedArray = mixed.split("");
shouldBe(indexOfTest(mixedArray, "a"), 0);
shouldBe(indexOfTest(mixedArray, "あ"), 1);
shouldBe(indexOfTest(mixedArray, "b"), 2);

// GC must not invalidate single-character lookups.
gc();
let postGCArray = dynamic.split("");
shouldBe(indexOfTest(postGCArray, "e"), 4);
shouldBe(indexOfTest(postGCArray, "Z"), -1);
