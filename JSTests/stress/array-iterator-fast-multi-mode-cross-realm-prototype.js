// FastArrayValues / FastArrayKeys / FastArrayEntries reuse the input iterator (m_iterator =
// m_iterable) instead of constructing a fresh one. Therefore the iterator carries the
// iterable's realm, and OUR realm's arrayIteratorProtocolWatchpointSet only guards the
// iterator if it actually lives in OUR realm.
//
// In the multi-mode dispatch we used to test IsCellWithType(JSArrayIteratorType) on the
// iterable. That accepts cross-realm JSArrayIterators with the same JSType, and combined
// with an own [Symbol.iterator] override pointing at our realm's
// iteratorProtoSymbolIteratorFunction, both the type check and the symbol-iterator
// CompareEqPtr would pass — and the fast path would silently bypass the OTHER realm's
// ArrayIteratorPrototype.next (which our watchpoint does NOT cover).
//
// MatchStructure pinning the iterable to OUR arrayIteratorStructure rejects that case via
// speculation fail, so iterator_next re-executes through the generic protocol and the
// (modified) cross-realm prototype takes effect.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' (expected ' + expected + ')');
}

function sumIter(iter) {
    var sum = 0;
    for (var v of iter) {
        if (typeof v === 'number')
            sum += v;
        else
            sum += v[0] + v[1]; // entries [k, v]
    }
    return sum;
}
noInline(sumIter);

// Train all three ArrayIterator kinds → multi-mode dispatch.
var arr = [1, 2, 3, 4, 5];
for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(sumIter(arr.values()), 15);
    shouldBe(sumIter(arr.keys()), 10);
    shouldBe(sumIter(arr.entries()), 25);
}

var otherRealm = createGlobalObject();
var otherRealmArray = otherRealm.Array(1, 2, 3, 4, 5);

// Replace the OTHER realm's ArrayIteratorPrototype.next so we can detect whether the fast
// path silently bypassed it. Multiplies each yielded value by 100.
var otherArrayIteratorPrototype = Object.getPrototypeOf(otherRealmArray.values());
otherArrayIteratorPrototype.next = function() {
    if (this.__idx === undefined) this.__idx = 0;
    if (this.__arr === undefined) return { value: undefined, done: true };
    if (this.__idx >= this.__arr.length) return { value: undefined, done: true };
    return { value: this.__arr[this.__idx++] * 100, done: false };
};

function makeAttackIter() {
    // Cross-realm ArrayIterator, but with OUR realm's Iterator.prototype[Symbol.iterator]
    // installed as an own property. Without MatchStructure, IsCellWithType(JSArrayIteratorType)
    // and CompareEqPtr(OUR iteratorProtoSymbolIteratorFunction) both pass — yet the iterator
    // structure is the OTHER realm's, so OUR per-realm watchpoint is the wrong watchpoint.
    var iter = otherRealm.Array.prototype.values.call(otherRealmArray);
    iter[Symbol.iterator] = Iterator.prototype[Symbol.iterator];
    iter.__arr = otherRealmArray;
    iter.__idx = 0;
    return iter;
}

// With MatchStructure pinning to OUR arrayIteratorStructure, the cross-realm iter
// speculation-fails out of the fast path → iterator_open re-executes via the generic
// protocol → iter.next dispatches through the modified cross-realm
// ArrayIteratorPrototype.next → values are multiplied by 100.
//
// (Pre-fix, with IsCellWithType, the fast path would have accepted this iterator and walked
// the underlying array directly, returning 15 and silently ignoring the modified prototype.)
for (var i = 0; i < testLoopCount; ++i)
    shouldBe(sumIter(makeAttackIter()), 1500); // 100+200+300+400+500

// Same-realm iteration must remain unaffected.
for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(sumIter(arr.values()), 15);
    shouldBe(sumIter(arr.keys()), 10);
    shouldBe(sumIter(arr.entries()), 25);
}

// Same-realm ArrayIterator with own [Symbol.iterator] override pointing to OUR realm's
// function: IsCellWithType + CompareEqPtr pass, but the own override transitions the
// iterator's structure away from arrayIteratorStructure → MatchStructure speculation-fails
// → generic protocol. Behavior must remain correct.
function values(iter) {
    var sum = 0;
    for (var v of iter)
        sum += v;
    return sum;
}
noInline(values);

for (var i = 0; i < testLoopCount; ++i) {
    var v = arr.values();
    v[Symbol.iterator] = Iterator.prototype[Symbol.iterator];
    shouldBe(values(v), 15);
}
