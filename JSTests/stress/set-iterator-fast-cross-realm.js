// Cross-realm: iterating a Set / SetIterator from another realm.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' (expected ' + expected + ')');
}

var otherRealm = createGlobalObject();

function valuesOfSet(set) {
    var sum = 0;
    for (var v of set.values()) sum += v;
    return sum;
}
noInline(valuesOfSet);

function plainOfSet(set) {
    var sum = 0;
    for (var v of set) sum += v;
    return sum;
}
noInline(plainOfSet);

function entriesOfSet(set) {
    var sum = 0;
    for (var [a, b] of set.entries()) sum += a + b;
    return sum;
}
noInline(entriesOfSet);

var otherSet = otherRealm.eval('var s = new Set(); for (var i = 0; i < 10; ++i) s.add(i + 1); s');

for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(valuesOfSet(otherSet), 55);
    shouldBe(plainOfSet(otherSet), 55);
    shouldBe(entriesOfSet(otherSet), 110);
}
