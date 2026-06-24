// Adversarial coverage for %RegExpStringIteratorPrototype%.next, focused on the DFG/FTL
// RegExpStringIteratorNext intrinsic: fast/slow path transitions, GC interleaving, OSR exit on
// brand-check failures, Unicode boundary advancement, reentrancy, and the exact shape of the
// inline-allocated iterator result object. Passing tests produce no output.

function shouldBe(actual, expected) {
    if (JSON.stringify(actual) !== JSON.stringify(expected))
        throw new Error("Expected " + JSON.stringify(expected) + " but got " + JSON.stringify(actual));
}

function shouldThrow(fn, ctor) {
    let threw = false;
    try { fn(); } catch (e) {
        threw = true;
        if (ctor && !(e instanceof ctor))
            throw new Error("Expected " + ctor.name + ", got " + e);
    }
    if (!threw)
        throw new Error("Expected throw");
}

// Captures the matcher that String.prototype.matchAll clones via @@species, so the test can poke
// at the exact RegExp object the iterator iterates over.
function matchAllCapturing(string, pattern, flags, configure) {
    let matcher;
    class Species extends RegExp {
        static get [Symbol.species]() {
            return function (p, f) {
                matcher = new RegExp(p, f);
                if (configure)
                    configure(matcher);
                return matcher;
            };
        }
    }
    const iterator = string.matchAll(new Species(pattern, flags));
    return { iterator, matcher };
}

const RegExpStringIteratorPrototype = Object.getPrototypeOf("".matchAll(/x/g));
const rawNext = RegExpStringIteratorPrototype.next;

// 1. Hot call site stays correct when the matcher's `exec` is swapped mid-iteration: each next()
//    re-checks the exec watchpoint, so installing an own `exec` after the fast path tiered up must
//    fall back to the observable slow path.
{
    function drain(iterator) {
        let count = 0;
        for (;;) {
            if (iterator.next().done)
                return count;
            count++;
        }
    }
    noInline(drain);

    // Tier the call site up against primordial iterators.
    for (let i = 0; i < 1e5; ++i) {
        if (drain("a1b2c3d4".matchAll(/[a-z]/g)) !== 4)
            throw new Error("warmup mismatch");
    }

    const { iterator, matcher } = matchAllCapturing("abcdef", "[a-z]", "g");
    // First two pulls go through the primordial fast path.
    shouldBe(iterator.next().value[0], "a");
    shouldBe(iterator.next().value[0], "b");

    // Install an own exec: subsequent pulls must observe it, even on the compiled call site.
    let execCalls = 0;
    matcher.exec = function (str) {
        execCalls++;
        return RegExp.prototype.exec.call(this, str);
    };
    shouldBe(iterator.next().value[0], "c");
    shouldBe(execCalls, 1);

    // Remove it again: back to the fast path, no further exec calls.
    delete matcher.exec;
    shouldBe(iterator.next().value[0], "d");
    shouldBe(execCalls, 1);
}

// 2. GC interleaved with a compiled fast path must not lose the freshly produced match object: the
//    intrinsic allocates the result object after the operation returns the match.
{
    function pull(iterator) {
        return iterator.next();
    }
    noInline(pull);

    for (let i = 0; i < 1e5; ++i)
        pull("xyz".matchAll(/./g));

    for (let i = 0; i < 200; ++i) {
        const iterator = ("p" + i + "q" + i).matchAll(/[a-z]\d+/g);
        const result = pull(iterator);
        edenGC();
        // The match array and its captured substring must survive the collection.
        if (result.value[0][0] !== "p" && result.value[0][0] !== "q")
            throw new Error("match clobbered by GC: " + result.value[0]);
        fullGC();
        shouldBe(pull(iterator).value[0][0], "q");
    }
}

// 3. A custom exec (slow path) that allocates heavily and triggers GC before returning the match
//    must still hand back an intact match through the intrinsic's result-object construction.
{
    function pull(iterator) {
        return iterator.next();
    }
    noInline(pull);

    for (let i = 0; i < 1e5; ++i)
        pull("abc".matchAll(/./g));

    const { iterator } = matchAllCapturing("hello", "l", "g", (m) => {
        let calls = 0;
        m.exec = function () {
            const garbage = [];
            for (let i = 0; i < 5000; ++i)
                garbage.push({ i, s: "g" + i });
            gc();
            if (calls++ === 0)
                return Object.assign(["LL"], { index: 2, input: "hello", length: 1 });
            return null;
        };
    });
    const result = pull(iterator);
    shouldBe(result.done, false);
    shouldBe(result.value[0], "LL");
    shouldBe(pull(iterator), { value: undefined, done: true });
}

// 4. After the brand check tiers up, non-RegExp-String-Iterator receivers must OSR-exit and throw
//    TypeError. Other internal-field iterators are objects but a different cell type.
{
    function callNext(receiver) {
        return rawNext.call(receiver);
    }
    noInline(callNext);

    for (let i = 0; i < 1e5; ++i) {
        if (callNext("ab".matchAll(/a/g)).done !== false)
            throw new Error("warmup mismatch");
    }

    const otherIterators = [
        [][Symbol.iterator](),
        new Map()[Symbol.iterator](),
        new Set()[Symbol.iterator](),
        (function* () {})(),
        "abc"[Symbol.iterator](),
    ];
    for (let i = 0; i < 1000; ++i) {
        for (const other of otherIterators)
            shouldThrow(() => callNext(other), TypeError);
        // Interleave a valid receiver so the call site stays polymorphic, not just deoptimized.
        shouldBe(callNext("z".matchAll(/z/g)).done, false);
    }
}

// 5. Unicode empty-match advancement at string boundaries and across rope joins.
{
    // Lone leading high surrogate: advances one code unit, not two.
    shouldBe([...("\uD83D".matchAll(/(?:)/gu))].map((m) => m.index), [0, 1]);
    // Lone trailing low surrogate.
    shouldBe([...("\uDE00".matchAll(/(?:)/gu))].map((m) => m.index), [0, 1]);
    // A valid surrogate pair assembled as a rope must still advance as one code point.
    const rope = "\uD83D" + "\uDE00";
    shouldBe([...(rope.matchAll(/(?:)/gu))].map((m) => m.index), [0, 2]);
    // Without 'u', the same rope is two code units.
    shouldBe([...(rope.matchAll(/(?:)/g))].map((m) => m.index), [0, 1, 2]);
    // Empty match exactly at the end of a unicode string terminates without reading past the end.
    shouldBe([...("\u{1F600}".matchAll(/(?:)/gu))].map((m) => m.index), [0, 2]);
}

// 6. ToLength clamps an out-of-range lastIndex in the empty-match slow path; advancement and the
//    written-back lastIndex use 64-bit arithmetic.
{
    let execCalls = 0;
    const { iterator, matcher } = matchAllCapturing("anything", "a", "g", (m) => {
        m.exec = function () {
            return execCalls++ === 0 ? Object.assign([""], { index: 0, input: "anything", length: 1 }) : null;
        };
    });
    matcher.lastIndex = Number.MAX_SAFE_INTEGER; // 2**53 - 1
    shouldBe(iterator.next().done, false);
    // ToLength(2**53 - 1) = 2**53 - 1; AdvanceStringIndex (non-unicode) adds one.
    shouldBe(matcher.lastIndex, 2 ** 53);
    shouldBe(iterator.next(), { value: undefined, done: true });
}

// 7. A throwing ToLength(Get(R, "lastIndex")) during the empty-match slow path propagates, and the
//    iterator is not finished afterward (the abrupt completion happens before [[Done]] would be set
//    for a global). lastIndex is a non-configurable but writable data property, so the side effect
//    is injected through a coercion-throwing value rather than an accessor.
{
    const { iterator, matcher } = matchAllCapturing("abc", "a", "g", (m) => {
        m.exec = function () { return Object.assign([""], { index: 0, input: "abc", length: 1 }); };
    });
    matcher.lastIndex = { valueOf() { throw new RangeError("boom"); } };
    shouldThrow(() => iterator.next(), RangeError);
    // Still not done: another pull re-enters exec and throws again rather than reporting done.
    shouldThrow(() => iterator.next(), RangeError);
}

// 8. The inline-allocated result object must be an ordinary object with plain, fully mutable
//    value/done data properties, identical to CreateIteratorResultObject, even after FTL warmup.
{
    function pull(iterator) {
        return iterator.next();
    }
    noInline(pull);

    for (let i = 0; i < 1e5; ++i)
        pull("ab".matchAll(/a/g));

    function assertResultShape(result) {
        shouldBe(Object.getPrototypeOf(result), Object.prototype);
        shouldBe(Reflect.ownKeys(result), ["value", "done"]);
        const v = Object.getOwnPropertyDescriptor(result, "value");
        const d = Object.getOwnPropertyDescriptor(result, "done");
        shouldBe([v.writable, v.enumerable, v.configurable], [true, true, true]);
        shouldBe([d.writable, d.enumerable, d.configurable], [true, true, true]);
        // Mutating one result must not affect later results (distinct objects).
        result.value = "tampered";
        result.done = "tampered";
    }

    // Has-match result.
    assertResultShape(pull("a".matchAll(/a/g)));
    // Done result.
    const finished = "a".matchAll(/a/g);
    pull(finished);
    assertResultShape(pull(finished));

    // The structure transition must be shared, not poisoned by the tampering above.
    const fresh = pull("b".matchAll(/b/g));
    shouldBe(fresh.value[0], "b");
    shouldBe(fresh.done, false);
}

// 9. Reentrancy: a nested iteration driven from inside another iterator's slow-path exec must not
//    corrupt the outer iterator's state.
{
    const inner = "12345".matchAll(/\d/g);
    let innerSum = 0;
    const { iterator: outer } = matchAllCapturing("abc", "[a-z]", "g", (m) => {
        let calls = 0;
        m.exec = function (str) {
            // Pull one element from a different live iterator on every exec.
            const step = inner.next();
            if (!step.done)
                innerSum += Number(step.value[0]);
            return calls++ < 3 ? RegExp.prototype.exec.call(this, str) : null;
        };
    });
    const outerMatches = [...outer];
    shouldBe(outerMatches.map((m) => m[0]), ["a", "b", "c"]);
    // exec ran 4 times (3 matches + final null), pulling 4 elements from inner: 1+2+3+4.
    shouldBe(innerSum, 10);
}
