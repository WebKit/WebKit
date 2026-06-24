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

// Warm up both String.prototype.search and RegExp.prototype[Symbol.search]
// before making any lastIndex non-writable, so both get constant-folded while
// the realm's "RegExp lastIndex is writable" watchpoint is still intact.
const re1 = /b/;
re1.lastIndex = 3;
function stringSearch() {
    return "abc".search(re1);
}
noInline(stringSearch);

const re2 = /b/;
re2.lastIndex = 3;
function symbolSearch() {
    return re2[Symbol.search]("abc");
}
noInline(symbolSearch);

for (var i = 0; i < testLoopCount; i++) {
    shouldBe(stringSearch(), 1);
    shouldBe(symbolSearch(), 1);
}

// Making lastIndex non-writable after tier-up must throw a TypeError because
// RegExp.prototype[@@search] sets lastIndex to 0 when it is not already 0.
Object.defineProperty(re1, "lastIndex", { writable: false });
for (var i = 0; i < 10; i++) {
    shouldThrow(() => stringSearch(), TypeError);
    shouldBe(symbolSearch(), 1); // re2's lastIndex is still writable.
}

Object.defineProperty(re2, "lastIndex", { writable: false });
for (var i = 0; i < 10; i++)
    shouldThrow(() => symbolSearch(), TypeError);

// A search becoming hot after the watchpoint already fired: the fold has to
// use a runtime guard instead, and flipping writability must still throw.
const re4 = /b/;
re4.lastIndex = 3;
function lateSearch() {
    return "abc".search(re4);
}
noInline(lateSearch);

for (var i = 0; i < testLoopCount; i++)
    shouldBe(lateSearch(), 1);

Object.defineProperty(re4, "lastIndex", { writable: false });
for (var i = 0; i < 10; i++)
    shouldThrow(() => lateSearch(), TypeError);

// When lastIndex is already 0, @@search never needs to write lastIndex for a
// non-global RegExp, so a non-writable lastIndex must not throw.
const re3 = /b/;
Object.defineProperty(re3, "lastIndex", { value: 0, writable: false });
function nonThrowingSearch() {
    return "abc".search(re3);
}
noInline(nonThrowingSearch);

for (var i = 0; i < testLoopCount; i++)
    shouldBe(nonThrowingSearch(), 1);
