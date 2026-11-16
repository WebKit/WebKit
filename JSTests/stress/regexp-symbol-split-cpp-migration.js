// Coverage for RegExp.prototype[Symbol.split] now that it is implemented in C++.
// Tests in this file should crash or throw on failure; passing tests must produce no output.

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

const splitFn = RegExp.prototype[Symbol.split];
shouldBe(splitFn.length, 2);
shouldBe(splitFn.name, "[Symbol.split]");
shouldBe(Object.getOwnPropertyDescriptor(RegExp.prototype, Symbol.split).enumerable, false);

// Step 1-2: receiver must be Object.
shouldThrow(() => splitFn.call(undefined, "a"), TypeError);
shouldThrow(() => splitFn.call(null, "a"), TypeError);
shouldThrow(() => splitFn.call(42, "a"), TypeError);
shouldThrow(() => splitFn.call("not an object", "a"), TypeError);

// Step 3: ToString called on the input.
{
    let calls = 0;
    const arg = { toString() { calls++; return "x,y,z"; } };
    shouldBe(splitFn.call(/,/, arg), ["x", "y", "z"]);
    shouldBe(calls, 1);
}

// Step 4: SpeciesConstructor — null/undefined fall back to %RegExp%, function species (non-constructor) throws,
// and a custom constructor is invoked with (rx, newFlags).
{
    class WithNullSpecies extends RegExp {
        static get [Symbol.species]() { return null; }
    }
    shouldBe(splitFn.call(new WithNullSpecies(/,/), "a,b"), ["a", "b"]);

    class WithUndefinedSpecies extends RegExp {
        static get [Symbol.species]() { return undefined; }
    }
    shouldBe(splitFn.call(new WithUndefinedSpecies(/,/), "p,q,r"), ["p", "q", "r"]);

    const r = /,/;
    r.constructor = { [Symbol.species]: () => null };
    shouldThrow(() => splitFn.call(r, "a,b"), TypeError);

    let species;
    let speciesArgs;
    class CustomSpecies extends RegExp {
        static get [Symbol.species]() {
            return species;
        }
    }
    species = function CustomCtor(rx, newFlags) {
        speciesArgs = [rx, newFlags];
        // Forward to RegExp so we still get a usable splitter.
        return new RegExp(rx, newFlags);
    };
    splitFn.call(new CustomSpecies(/,/, "g"), "a,b");
    if (!(speciesArgs[0] instanceof RegExp))
        throw new Error("species: rx not RegExp");
    if (!speciesArgs[1].includes("y"))
        throw new Error("species: newFlags must contain 'y', got " + speciesArgs[1]);
}

// Step 5: ToString called on regexp.flags.
{
    let calls = 0;
    const r = /,/;
    Object.defineProperty(r, "flags", {
        get() { calls++; return ""; }
    });
    splitFn.call(r, "a,b,c");
    if (calls < 1)
        throw new Error("flags getter not called");
}

// Step 5: an exception in flags propagates.
{
    const r = /,/;
    Object.defineProperty(r, "flags", {
        get() { throw new Error("flags-bang"); }
    });
    try {
        splitFn.call(r, "a,b,c");
        throw new Error("should throw");
    } catch (e) {
        if (e.message !== "flags-bang")
            throw new Error("expected flags-bang, got " + e);
    }
}

// Step 6: 'u' or 'v' triggers unicode AdvanceStringIndex (surrogate pair stays together).
{
    const surrogate = "a😀b";
    shouldBe(splitFn.call(/(?:)/u, surrogate), ["a", "😀", "b"]);
    shouldBe(splitFn.call(/(?:)/v, surrogate), ["a", "😀", "b"]);
    // Without u/v the surrogate is split at the code-unit boundary.
    shouldBe(splitFn.call(/(?:)/, surrogate), ["a", "\uD83D", "\uDE00", "b"]);
}

// Step 7: 'y' is added to flags if missing — triggers sticky exec on the splitter.
// We can't observe newFlags directly without species, but a sticky no-match means split returns the whole string.
{
    // /a/ matches at any offset, but the splitter has /ay/, which from position 0 in "ba" doesn't match.
    // (Our advanceStringIndex still moves past 'b', then the next attempt at 1 matches 'a'.)
    shouldBe(splitFn.call(/a/, "ba"), ["b", ""]);
}

// Step 8 (Construct splitter) propagates errors.
{
    class BadCtor extends RegExp {
        constructor() { throw new Error("ctor-bang"); }
        static get [Symbol.species]() { return BadCtor; }
    }
    try {
        // Use a primordial RegExp that will go through species path because flags getter is overridden.
        const r = /,/;
        Object.setPrototypeOf(r, BadCtor.prototype);
        r.constructor = BadCtor;
        splitFn.call(r, "a,b");
        throw new Error("should throw");
    } catch (e) {
        if (e.message !== "ctor-bang")
            throw new Error("expected ctor-bang, got " + e);
    }
}

// Step 11 / 12: limit handling.
shouldBe(splitFn.call(/,/, "a,b,c", 0), []);
shouldBe(splitFn.call(/,/, "a,b,c", 1), ["a"]);
shouldBe(splitFn.call(/,/, "a,b,c", 2), ["a", "b"]);
shouldBe(splitFn.call(/,/, "a,b,c", undefined), ["a", "b", "c"]);
// ToUint32(-1) wraps to 0xFFFFFFFF, so the limit is effectively unlimited.
shouldBe(splitFn.call(/,/, "a,b,c", -1), ["a", "b", "c"]);
{
    let calls = 0;
    const limit = { valueOf() { calls++; return 2; } };
    shouldBe(splitFn.call(/,/, "a,b,c", limit), ["a", "b"]);
    shouldBe(calls, 1);
}

// limit value coerced to UInt32 — ToUint32(2.7) = 2.
shouldBe(splitFn.call(/,/, "a,b,c,d", 2.7), ["a", "b"]);
// Limit equal to total parts.
shouldBe(splitFn.call(/,/, "a,b,c", 3), ["a", "b", "c"]);
// Limit larger than total parts.
shouldBe(splitFn.call(/,/, "a,b,c", 100), ["a", "b", "c"]);

// Step 13: empty string.
shouldBe(splitFn.call(/x/, ""), [""]);   // exec returns null.
shouldBe(splitFn.call(/(?:)/, ""), []);  // exec returns a match.

// Step 17 capture-group cases.
shouldBe(splitFn.call(/(b)/, "abc"), ["a", "b", "c"]);
shouldBe(splitFn.call(/(d)?b/, "abc"), ["a", undefined, "c"]);
shouldBe(splitFn.call(/(?<x>b)/, "abc"), ["a", "b", "c"]);
shouldBe(splitFn.call(/(b)(c)/, "abc"), ["a", "b", "c", ""]);

// Step 17.d.iii: e == p case — match at p with zero-length consumes nothing, advance by one.
shouldBe(splitFn.call(/(?=,)/, "a,b,c"), ["a", ",b", ",c"]);
shouldBe(splitFn.call(/(?:)/, "abc"), ["a", "b", "c"]);

// Custom exec via subclass: must round-trip results through user-facing array protocol.
{
    let execCalls = 0;
    class TraceExec extends RegExp {
        exec(s) {
            execCalls++;
            return RegExp.prototype.exec.call(this, s);
        }
    }
    const re = new TraceExec(/,/);
    shouldBe(splitFn.call(re, "a,b,c"), ["a", "b", "c"]);
    if (execCalls < 3)
        throw new Error("exec must be invoked at least once per non-final segment, got " + execCalls);
}

// exec returning non-null, non-Object throws TypeError.
{
    class BadExec extends RegExp {
        exec() { return 42; }
    }
    shouldThrow(() => splitFn.call(new BadExec(/,/), "a,b"), TypeError);
}

// exec returning null forwards to AdvanceStringIndex; should consume the whole string into a single segment.
{
    class NullExec extends RegExp {
        exec() { return null; }
    }
    shouldBe(splitFn.call(new NullExec(/,/), "a,b,c"), ["a,b,c"]);
}

// Step 17.d.i: ToLength(splitter.lastIndex) — receiver-controlled lastIndex, clamped to size.
{
    class LastIndexHack extends RegExp {
        exec(s) {
            const r = RegExp.prototype.exec.call(this, s);
            if (r) this.lastIndex = 1000;  // out of bounds, clamps to size.
            return r;
        }
    }
    // After the first match, e clamps to size, so p jumps to size and the trailing "" is pushed.
    shouldBe(splitFn.call(new LastIndexHack(/,/), "a,b,c"), ["a", ""]);
}

// Step 17.d.ii: large captures should all be returned, capped at limit.
{
    class FakeArray extends RegExp {
        exec(s) {
            const r = RegExp.prototype.exec.call(this, s);
            if (!r) return null;
            r.length = 4;
            r[1] = "X";
            r[2] = "Y";
            r[3] = "Z";
            return r;
        }
    }
    shouldBe(splitFn.call(new FakeArray(/,/), "a,b"), ["a", "X", "Y", "Z", "b"]);
    shouldBe(splitFn.call(new FakeArray(/,/), "a,b", 3), ["a", "X", "Y"]);
    shouldBe(splitFn.call(new FakeArray(/,/), "a,b", 4), ["a", "X", "Y", "Z"]);
}

// Sticky source: 'y' already present is preserved (no double 'y').
{
    let observedFlags;
    class SeeFlags extends RegExp {
        get flags() {
            observedFlags = super.flags;
            return observedFlags;
        }
    }
    splitFn.call(new SeeFlags(/a/y), "ba");
    if (observedFlags !== "y")
        throw new Error("Expected y flag preserved, got " + observedFlags);
}

// String.prototype.split(regexp) — sanity check that the C++ split is reachable through the
// String.prototype.split bridge (already tested elsewhere, but include a smoke check here).
shouldBe("a,b,c".split(/,/), ["a", "b", "c"]);
shouldBe("a,b,c".split(/,/, 1), ["a"]);

// Also check that the fast path triggers (instanceof RegExp + plain limit).
{
    const re = /,/;
    for (let i = 0; i < 10000; i++)
        re[Symbol.split]("a,b,c,d,e,f,g");
}
