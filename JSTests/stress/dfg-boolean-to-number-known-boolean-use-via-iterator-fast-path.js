// Regression test for SpeculativeJIT and FTL's BooleanToNumber: the switch on child1's UseKind
// previously only handled BooleanUse and UntypedUse, crashing with "Bad use kind" when fixup
// upgraded BooleanUse to KnownBooleanUse (e.g., when the input was a CompareStrictEq on Int32
// inputs and flowed through an immediate SetLocal at a non-exitOK position).
//
// The iterator-fast-path open emits exactly that pattern when seenModes contains both a Fast*
// kind and Generic — recompilation after OSR exit hits the multi-mode dispatch. This test
// forces that recompilation by mutating an iterator's structure mid-iteration.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' (expected ' + expected + ')');
}

function sumValuesIterator(iter) {
    var sum = 0;
    for (var v of iter)
        sum += v;
    return sum;
}
noInline(sumValuesIterator);

function sumKeysIterator(iter) {
    var sum = 0;
    for (var k of iter)
        sum += k;
    return sum;
}
noInline(sumKeysIterator);

function sumEntriesIterator(iter) {
    var sum = 0;
    for (var [k, v] of iter)
        sum += k + v;
    return sum;
}
noInline(sumEntriesIterator);

var array = [];
for (var i = 0; i < 10; ++i)
    array.push(i + 1);

// Warm up under FastArrayValues/Keys/Entries.
for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(sumValuesIterator(array.values()), 55);
    shouldBe(sumKeysIterator(array.keys()), 45);
    shouldBe(sumEntriesIterator(array.entries()), 100); // (0+1)+(1+2)+...+(9+10)
}

// Mutate to invalidate CheckStructure in the FastArrayValues path → OSR exit → recompile with
// both FastArrayValues and Generic in seenModes.
var customValues = array.values();
customValues[Symbol.iterator] = function () { return [777][Symbol.iterator](); };
shouldBe(sumValuesIterator(customValues), 777);

var customKeys = array.keys();
customKeys[Symbol.iterator] = function () { return [111][Symbol.iterator](); };
shouldBe(sumKeysIterator(customKeys), 111);

var customEntries = array.entries();
customEntries[Symbol.iterator] = function () { return [[0, 222]][Symbol.iterator](); };
shouldBe(sumEntriesIterator(customEntries), 222);

// More iterations after recompilation. Without the SpeculativeJIT/FTL fix for KnownBooleanUse on
// BooleanToNumber, the recompile crashes with "Bad use kind" while compiling the dispatch
// ArithBitAnd in the multi-mode iterator-open path.
for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(sumValuesIterator(array.values()), 55);
    shouldBe(sumKeysIterator(array.keys()), 45);
    shouldBe(sumEntriesIterator(array.entries()), 100);
}
