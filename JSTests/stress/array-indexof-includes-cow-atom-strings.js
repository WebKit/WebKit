// indexOf/includes on copy-on-write "only atom strings" array literals, with a fallback
// when the search element is not an atom.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: expected ${expected} but got ${actual}`);
}

function idx(s) { return ['__flags', '__methods', '_obj', 'assert'].indexOf(s); }
noInline(idx);

function idxFrom(s, from) { return ['__flags', '__methods', '_obj', 'assert'].indexOf(s, from); }
noInline(idxFrom);

function inc(s) { return ['__flags', '__methods', '_obj', 'assert'].includes(s); }
noInline(inc);

function idxEmptyAndSingle(s) { return ['', 'a', 'bb'].indexOf(s); }
noInline(idxEmptyAndSingle);

for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(idx('__flags'), 0);
    shouldBe(idx('__methods'), 1);
    shouldBe(idx('_obj'), 2);
    shouldBe(idx('assert'), 3);

    shouldBe(idx('nope'), -1);
    shouldBe(idx('then'), -1);

    shouldBe(inc('__methods'), true);
    shouldBe(inc('xyz'), false);

    // Non-atom search string equal to an element must hit the slow path and return the right index.
    const dyn = '_o' + 'bj';
    shouldBe(idx(dyn), 2);
    shouldBe(inc(dyn), true);
    shouldBe(idx('as' + 'sert'), 3);
    shouldBe(idx('no' + 'match'), -1);

    shouldBe(idxFrom('__flags', 1), -1);
    shouldBe(idxFrom('assert', 2), 3);
    shouldBe(idxFrom('_obj', 3), -1);

    shouldBe(idxEmptyAndSingle(''), 0);
    shouldBe(idxEmptyAndSingle('a'), 1);
    shouldBe(idxEmptyAndSingle('bb'), 2);
    shouldBe(idxEmptyAndSingle('' + ''), 0);
    shouldBe(idxEmptyAndSingle(String.fromCharCode(97)), 1);
    shouldBe(idxEmptyAndSingle('z'), -1);
}
