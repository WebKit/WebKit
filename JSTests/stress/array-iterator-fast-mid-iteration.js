// Mid-iteration: pause an iterator with N values consumed, then continue via for..of which calls
// next() on the partially-advanced iterator. Verify the FastArrayValues/Keys/Entries path correctly
// resumes from where the manual .next() left off.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' (expected ' + expected + ')');
}

function consumeOneAndIterateRest(iter) {
    var first = iter.next();   // manually consume one
    var rest = 0;
    for (var v of iter)
        rest += v;
    return [first.value, first.done, rest];
}
noInline(consumeOneAndIterateRest);

var array = [];
for (var i = 0; i < 10; ++i)
    array.push(i + 1);

for (var i = 0; i < testLoopCount; ++i) {
    var [first, done, rest] = consumeOneAndIterateRest(array.values());
    shouldBe(first, 1);
    shouldBe(done, false);
    shouldBe(rest, 54); // 2+3+...+10
}

// Same with keys.
function consumeKeysRest(iter) {
    var first = iter.next();
    var rest = 0;
    for (var k of iter)
        rest += k;
    return [first.value, rest];
}
noInline(consumeKeysRest);

for (var i = 0; i < testLoopCount; ++i) {
    var [first, rest] = consumeKeysRest(array.keys());
    shouldBe(first, 0);
    shouldBe(rest, 45); // 1+2+...+9
}

// Same with entries.
function consumeEntriesRest(iter) {
    var first = iter.next();
    var rest = 0;
    for (var [k, v] of iter)
        rest += k * 1000 + v;
    return [first.value[0], first.value[1], rest];
}
noInline(consumeEntriesRest);

var expectedRest = 0;
for (var i = 1; i < 10; ++i)
    expectedRest += i * 1000 + (i + 1);

for (var i = 0; i < testLoopCount; ++i) {
    var [k, v, rest] = consumeEntriesRest(array.entries());
    shouldBe(k, 0);
    shouldBe(v, 1);
    shouldBe(rest, expectedRest);
}

// Drain to done via manual .next() then attempt iteration — should yield zero rest.
function drainThenIterate(iter) {
    while (!iter.next().done) {}
    var rest = 0;
    for (var v of iter)
        rest += v;
    return rest;
}
noInline(drainThenIterate);

for (var i = 0; i < testLoopCount; ++i)
    shouldBe(drainThenIterate(array.values()), 0);
