function shouldBe(actual, expected) {
    if (!Object.is(actual, expected))
        throw new Error("bad value: " + actual + " expected: " + expected);
}

var allowDoubleShape = $vm.allowDoubleShape();

// Growing the length within the vector capacity exposes the unused slots past the parsed
// elements without rewriting them, so each of them has to read back as a hole.
function checkTail(array, length) {
    for (var extra = 1; extra <= 16; ++extra) {
        array.length = length + extra;
        for (var i = length; i < length + extra; ++i) {
            shouldBe(i in array, false);
            shouldBe(array[i], undefined);
        }
    }
    array.length = length;
}

var doubleMode = allowDoubleShape ? "ArrayWithDouble" : "ArrayWithContiguous";
var kinds = [
    { element: i => String(i), value: i => i, mode: () => "ArrayWithInt32" },
    { element: i => String(i + 0.5), value: i => i + 0.5, mode: () => doubleMode },
    { element: i => String(i % 2 ? i + 0.5 : i), value: i => i % 2 ? i + 0.5 : i, mode: length => length > 1 ? doubleMode : "ArrayWithInt32" },
    { element: i => '"' + i + '"', value: i => String(i), mode: () => "ArrayWithContiguous" },
    { element: i => i % 2 ? "null" : String(i), value: i => i % 2 ? null : i, mode: length => length > 1 ? "ArrayWithContiguous" : "ArrayWithInt32" },
];

var lengths = [];
for (var length = 1; length <= 40; ++length)
    lengths.push(length);
lengths.push(63, 64, 65, 127, 128, 129, 1000);

function run(expectedModes) {
    for (var iteration = 0; iteration < 20; ++iteration) {
        // Reused butterflies still hold earlier elements, so an uncleared slot reads back as a value
        // instead of as the zero that a fresh page would show.
        fullGC();
        for (var kind of kinds) {
            for (var length of lengths) {
                var source = "[" + Array.from({ length }, (_, i) => kind.element(i)).join(",") + "]";
                var array = JSON.parse(source);
                // Any other access first could let the JIT convert the array to a more general shape.
                if (expectedModes)
                    shouldBe($vm.indexingMode(array), kind.mode(length));
                shouldBe(array.length, length);
                for (var i = 0; i < length; ++i)
                    shouldBe(array[i], kind.value(i));
                checkTail(array, length);
            }
        }
    }
}

run(!$vm.isHavingABadTime(globalThis));

// Once an indexed accessor on Array.prototype makes the global object have a bad time, arrays are
// allocated with array storage, where the slots past the parsed elements must still be holes.
Object.defineProperty(Array.prototype, "100000", { get() { return "bad"; }, configurable: true });
run(false);
