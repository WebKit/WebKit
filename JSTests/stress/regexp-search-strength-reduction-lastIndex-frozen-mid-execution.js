//@ requireOptions("--useConcurrentJIT=0")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Expected ${expected} but got ${actual}`);
}

function shouldThrow(run, errorType) {
    let actual;
    let hadError = false;
    try {
        actual = run();
    } catch (e) {
        hadError = true;
        actual = e;
    }
    if (!hadError)
        throw new Error(`Expected ${run}() to throw ${errorType.name}, but did not throw.`);
    if (!(actual instanceof errorType))
        throw new Error(`Expected ${run}() to throw ${errorType.name}, but threw '${actual}'`);
}

// The regexp gets frozen between its creation and the constant-folded search,
// inside the same function execution. The "RegExp lastIndex is writable"
// watchpoint fires mid-execution, so the code must deoptimize at the
// invalidation point after the call and the search must throw a TypeError.
function search(callback) {
    const re = /b/;
    callback(re);
    return "abc".search(re);
}
noInline(search);

function benign(re) {
}

for (var i = 0; i < testLoopCount; i++)
    shouldBe(search(benign), 1);

shouldThrow(() => search((re) => {
    re.lastIndex = 3;
    Object.freeze(re);
}), TypeError);

// A fresh regexp in later calls is unaffected, even though the realm's
// watchpoint has fired by now.
for (var i = 0; i < 10; i++)
    shouldBe(search(benign), 1);
