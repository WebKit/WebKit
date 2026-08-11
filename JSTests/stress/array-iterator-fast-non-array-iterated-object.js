// JSArrayIterator created over a non-JSArray (TypedArray) must NOT take the FastArrayValues fast
// path. The sentinel cell in op_iterator_open is set only when iteratedObject is a plain JSArray;
// for any other iteratedObject (TypedArray, arguments, plain object) we must fall through to the
// generic iteration protocol — never run the unsafe code that assumes JSArray.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' (expected ' + expected + ')');
}

function sumValues(iter) {
    var sum = 0;
    for (var v of iter)
        sum += v;
    return sum;
}
noInline(sumValues);

function sumKeys(iter) {
    var sum = 0;
    for (var k of iter)
        sum += k;
    return sum;
}
noInline(sumKeys);

function sumEntries(iter) {
    var sum = 0;
    for (var [k, v] of iter)
        sum += k * 1000 + v;
    return sum;
}
noInline(sumEntries);

var typed = new Uint32Array(10);
for (var i = 0; i < 10; ++i)
    typed[i] = i + 1;

var expectedValues = 55;
var expectedKeys = 45;
var expectedEntries = 0;
for (var i = 0; i < 10; ++i)
    expectedEntries += i * 1000 + (i + 1);

// Plain typed-array iterators across all three kinds.
for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(sumValues(Array.prototype.values.call(typed)), expectedValues);
    shouldBe(sumKeys(Array.prototype.keys.call(typed)), expectedKeys);
    shouldBe(sumEntries(Array.prototype.entries.call(typed)), expectedEntries);
}

// Profile pollution: warm up sumValues with a plain-JSArray-backed iterator (FastArrayValues path),
// then feed it a typed-array-backed iterator. The fast path's sentinel must NOT have been set on
// the typed-array iterator (because iteratedObject is not a JSArray) — so this must hit the generic
// path and produce the right value without crashing.
function values(iter) {
    var sum = 0;
    for (var v of iter)
        sum += v;
    return sum;
}
noInline(values);

var array = [];
for (var i = 0; i < 10; ++i)
    array.push(i + 1);

// Warm-up: FastArrayValues path on a plain JSArray-backed iterator.
for (var i = 0; i < testLoopCount; ++i)
    shouldBe(values(array.values()), 55);

// Mix in: typed-array-backed iterator. Must fall through to generic, must not take the fast path.
for (var i = 0; i < testLoopCount; ++i)
    shouldBe(values(Array.prototype.values.call(typed)), 55);

// Mid-iteration .next() on a typed-array iterator — also must not take the fast path.
function consumeOneAndIterateRest(iter) {
    var first = iter.next();
    var rest = 0;
    for (var v of iter)
        rest += v;
    return [first.value, first.done, rest];
}
noInline(consumeOneAndIterateRest);

for (var i = 0; i < testLoopCount; ++i) {
    var [first, done, rest] = consumeOneAndIterateRest(Array.prototype.values.call(typed));
    shouldBe(first, 1);
    shouldBe(done, false);
    shouldBe(rest, 54); // 2+3+...+10
}

// Detached buffer: iterating an iterator whose typed array's buffer is detached must throw, not crash.
function iterateOrThrow(iter) {
    try {
        for (var v of iter) { /* */ }
        return "no-throw";
    } catch (e) {
        return e.constructor.name;
    }
}
noInline(iterateOrThrow);

var typedToDetach = new Uint32Array(new ArrayBuffer(40));
var detachIter = Array.prototype.values.call(typedToDetach);
typedToDetach.buffer.transfer(); // detaches
shouldBe(iterateOrThrow(detachIter), "TypeError");
