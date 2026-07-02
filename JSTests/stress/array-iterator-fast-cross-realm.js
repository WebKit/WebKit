// Cross-realm coverage for the FastArrayValues/Keys/Entries open paths.
//
// The fast-iteration protocol must filter out iterators that belong to a different global object
// (different arrayIteratorStructure), and must safely handle a current-realm iterator whose
// iteratedObject is a cross-realm JSArray.
//
// The DFG single-mode path uses CheckStructure(currentRealm.arrayIteratorStructure), and the
// multi-mode path uses CompareEqPtr(currentRealm.iteratorProto[Symbol.iterator]). Either way,
// a cross-realm JSArrayIterator whose [Symbol.iterator] resolves to OTHER realm's iteratorPrototype
// must NOT take the fast path. The iterator-open watchpoint added in handleIteratorOpen is the
// CURRENT realm's; we rely on these dynamic checks to reject other-realm iterators.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' (expected ' + expected + ')');
}

var otherRealm = createGlobalObject();

// Build a same-shape array in the other realm.
var otherRealmArray = otherRealm.Array(1, 2, 3, 4, 5);

// (1) Iterating a CROSS-REALM JSArrayIterator whose [Symbol.iterator] is other-realm's identity.
// Our fast-path checks must reject this and route to the generic protocol.
function valuesOfIterator(iter) {
    var sum = 0;
    for (var v of iter)
        sum += v;
    return sum;
}
noInline(valuesOfIterator);

// Warm up under the FastArrayValues path with a current-realm iter so seenModes has FastArrayValues.
// (Mixing in a cross-realm iter forces the multi-mode dispatch, the path that omits the
// CheckStructure on the iterator.)
var thisRealmArray = [1, 2, 3, 4, 5];
for (var i = 0; i < testLoopCount; ++i)
    shouldBe(valuesOfIterator(thisRealmArray.values()), 15);

// Cross-realm JSArrayIterator: must fall back to generic and produce the right value.
for (var i = 0; i < testLoopCount; ++i)
    shouldBe(valuesOfIterator(otherRealmArray.values()), 15);

// (2) A CURRENT-realm iterator whose iteratedObject is a CROSS-REALM JSArray. The iterator's
// [Symbol.iterator] is current-realm's identity (passes the function-pointer check), and the
// iteratedObject is a JSArray (any realm). The fast path inlines butterfly access: must work.
for (var i = 0; i < testLoopCount; ++i)
    shouldBe(valuesOfIterator(Array.prototype.values.call(otherRealmArray)), 15);

// Same with keys/entries.
function keysOfIterator(iter) {
    var sum = 0;
    for (var k of iter)
        sum += k;
    return sum;
}
noInline(keysOfIterator);

function entriesOfIterator(iter) {
    var sum = 0;
    for (var [k, v] of iter)
        sum += k * 100 + v;
    return sum;
}
noInline(entriesOfIterator);

// Warm up.
for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(keysOfIterator(thisRealmArray.keys()), 10); // 0+1+2+3+4
    shouldBe(entriesOfIterator(thisRealmArray.entries()), 1015); // sum k*100+v
}

// Now feed cross-realm-array-backed current-realm iterators (uses fast path inlining).
for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(keysOfIterator(Array.prototype.keys.call(otherRealmArray)), 10);
    shouldBe(entriesOfIterator(Array.prototype.entries.call(otherRealmArray)), 1015);
}

// And cross-realm iterators (must bail to generic).
for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(keysOfIterator(otherRealmArray.keys()), 10);
    shouldBe(entriesOfIterator(otherRealmArray.entries()), 1015);
}

// (3) Mid-iteration with a cross-realm iter: exercises the iter-as-iterable path too.
function consumeOneAndIterateRest(iter) {
    var first = iter.next();
    var rest = 0;
    for (var v of iter)
        rest += v;
    return [first.value, first.done, rest];
}
noInline(consumeOneAndIterateRest);

for (var i = 0; i < testLoopCount; ++i) {
    var [first, done, rest] = consumeOneAndIterateRest(otherRealmArray.values());
    shouldBe(first, 1);
    shouldBe(done, false);
    shouldBe(rest, 14); // 2+3+4+5
}

// (4) Modifying OTHER realm's Array.prototype[Symbol.iterator] must NOT fool the fast path on
// CURRENT realm's iterators. Our watchpoint is current-realm's, so other-realm modifications are
// invisible to us. The dynamic function-pointer check on the symbolIterator slot already
// distinguishes realms, so this is purely a defense-in-depth check.
otherRealm.Array.prototype[Symbol.iterator] = function () {
    var idx = 0, arr = this;
    return { next() { return idx < arr.length ? { value: arr[idx++] * 100, done: false } : { value: undefined, done: true }; } };
};

// Same-realm iteration unaffected.
for (var i = 0; i < testLoopCount; ++i)
    shouldBe(valuesOfIterator(thisRealmArray.values()), 15);

// for-of on cross-realm-array uses other-realm's Symbol.iterator: 100+200+...+500 = 1500.
function plainFor(arr) {
    var sum = 0;
    for (var v of arr) sum += v;
    return sum;
}
noInline(plainFor);

shouldBe(plainFor(otherRealmArray), 1500);
