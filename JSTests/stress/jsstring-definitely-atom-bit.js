function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error(`${message}: expected ${expected} but got ${actual}`);
}

// The per-cell bit is one-sided: set implies the string holds an AtomStringImpl, clear proves
// nothing. Every assertion below either checks that implication or checks a path required to set it.
function checkInvariant(string, message) {
    if ($vm.isDefinitelyAtomString(string) && !$vm.isAtomString(string))
        throw new Error(`${message}: per-cell bit set on a string that is not an atom`);
}

function makeString(n) {
    return "jsStringDefinitelyAtomPrefix" + n;
}

function makeRope(n) {
    return makeString(n) + "ropeTail";
}

// A string literal reaches JSString::create wrapping the AtomStringImpl of the bytecode generator's
// Identifier, so it is flagged before anything touches it. SmallStrings are built from
// AtomStringImpl::add and the empty string from StringImpl::empty(), so they are flagged too.
checkInvariant("multiCharacterStringLiteral", "string literal");
shouldBe($vm.isDefinitelyAtomString("multiCharacterStringLiteral"), true, "string literal is a definite atom");
shouldBe($vm.isDefinitelyAtomString("a"), true, "single character string is a definite atom");
shouldBe($vm.isDefinitelyAtomString(""), true, "empty string is a definite atom");

function exercise(n) {
    // Using a rope as a property key resolves it straight to an AtomStringImpl.
    let key = makeRope(n);
    checkInvariant(key, "rope before use as key");

    let object = {};
    object[key] = n;
    shouldBe($vm.isAtomString(key), true, "rope atomized by property key use");
    shouldBe($vm.isDefinitelyAtomString(key), true, "rope flagged by property key use");
    shouldBe(object[key], n, "property lookup by atomized rope");

    // Property names handed back by the runtime are built from the property table's AtomStringImpls.
    for (let name of Object.keys(object)) {
        shouldBe($vm.isAtomString(name), true, "Object.keys result is an atom");
        shouldBe($vm.isDefinitelyAtomString(name), true, "Object.keys result is a definite atom");
    }

    for (let name in object)
        shouldBe($vm.isDefinitelyAtomString(name), true, "for-in key is a definite atom");

    // A second, equal rope reaches JSString::toExistingAtomString, which finds the atom the first
    // key already created.
    let equalKey = makeRope(n);
    shouldBe(object[equalKey], n, "property lookup by an equal rope");
    shouldBe($vm.isDefinitelyAtomString(equalKey), true, "equal rope flagged by lookup");

    // Map hashes string keys without atomizing them, so the bit must stay clear here while lookup by
    // an equal string keeps working.
    let map = new Map();
    let mapKey = makeRope(n);
    map.set(mapKey, n);
    checkInvariant(mapKey, "Map key");
    shouldBe(map.get(makeRope(n)), n, "Map lookup by an equal string");

    // Strings that never get atomized must keep working, and must never be flagged.
    let plain = makeString(n).slice(1);
    checkInvariant(plain, "unatomized substring");
    shouldBe(plain === makeString(n).slice(1), true, "substring equality");

    let unusedRope = makeRope(n) + "unusedTail";
    checkInvariant(unusedRope, "unresolved rope");
    shouldBe(unusedRope.length, makeRope(n).length + "unusedTail".length, "rope length");
    shouldBe(unusedRope === makeRope(n) + "unusedTail", true, "rope equality");
}

for (let i = 0; i < testLoopCount; ++i) {
    exercise(i);
    if (!(i % 2000))
        gc();
}
