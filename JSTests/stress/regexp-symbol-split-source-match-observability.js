// Source/match observability checks for the C++ RegExp.prototype[Symbol.split] migration.
//
// Reference (ES2024 22.2.5.13 + 22.2.4.1):
//   When the spec runs `splitter = new %RegExp%(rx, newFlags)`, RegExp constructor step 6
//   matches before step 7 if `rx` has the [[RegExpMatcher]] internal slot. Step 6 reads
//   `rx.[[OriginalSource]]` directly — neither `rx.source` nor IsRegExp's Symbol.match
//   lookup affects construction for actual RegExp objects.
//
// The C++ fast path relies on this: it does not watch `RegExp.prototype.source`, and the
// `regExpPrimordialPropertiesWatchpointSet` does not watch `Symbol.toPrimitive` or any
// proxy-only path. This file pins those equivalences so any future refactor that breaks
// the assumption fails loudly.

function shouldBe(actual, expected, label) {
    if (JSON.stringify(actual) !== JSON.stringify(expected))
        throw new Error(`${label}: got ${JSON.stringify(actual)}, expected ${JSON.stringify(expected)}`);
}

const splitFn = RegExp.prototype[Symbol.split];

// Snapshot expected behavior on a fresh (untouched primordial) state.
shouldBe(splitFn.call(/,/, "a,b,c"), ["a","b","c"], "baseline");

// 1. Override RegExp.prototype.source -> invisible for primordial RegExp receivers.
//    Step 6 of the RegExp constructor uses [[OriginalSource]], so the splitter is built
//    from "," not "x". Result must equal the baseline.
{
    const restore = Object.getOwnPropertyDescriptor(RegExp.prototype, "source");
    let getterCalls = 0;
    Object.defineProperty(RegExp.prototype, "source", {
        get() { getterCalls++; return "x"; },
        configurable: true,
    });
    try {
        shouldBe(splitFn.call(/,/, "a,b,c"), ["a","b","c"], "prototype source override");
    } finally {
        Object.defineProperty(RegExp.prototype, "source", restore);
    }
}

// 2. Define an *own* `source` on the receiver -> the receiver's structure transitions, so
//    the C++ fast path bails (hasCustomProperties) and the slow path runs. The slow path
//    constructs the splitter via `new %RegExp%(rx, newFlags)`, and again step 6 wins on
//    the receiver's [[OriginalSource]]. Result must still match.
{
    const re = /,/;
    let getterCalls = 0;
    Object.defineProperty(re, "source", {
        get() { getterCalls++; return "x"; },
        configurable: true,
    });
    shouldBe(splitFn.call(re, "a,b,c"), ["a","b","c"], "own source override");
}

// 3. Override RegExp.prototype[Symbol.match] -> watched in regExpPrimordialPropertiesWatchpointSet.
//    Forces the slow path; result still matches because Symbol.match is consumed by IsRegExp,
//    which is overridden by step 6 for actual RegExp instances.
{
    const restore = Object.getOwnPropertyDescriptor(RegExp.prototype, Symbol.match);
    let getterCalls = 0;
    // Make sure to still return a truthy match function so split itself works correctly.
    Object.defineProperty(RegExp.prototype, Symbol.match, {
        get() { getterCalls++; return restore.value; },
        configurable: true,
    });
    try {
        shouldBe(splitFn.call(/,/, "a,b,c"), ["a","b","c"], "prototype Symbol.match override");
        if (!getterCalls)
            throw new Error("Symbol.match getter never fired -> fast path bypassed prototype watchpoint");
    } finally {
        Object.defineProperty(RegExp.prototype, Symbol.match, restore);
    }
}

// 4. Plain Object proxy without a [[RegExpMatcher]] slot. Now step 6 of the RegExp constructor
//    *does not* apply, so step 7's IsRegExp result and `source` getter are observed. This
//    case forces the slow path automatically because the receiver isn't a RegExpObject.
{
    const accesses = [];
    const inner = /it/;
    const proxy = new Proxy(inner, {
        get(obj, prop) {
            accesses.push(prop.toString());
            if (prop === Symbol.match)
                return undefined; // Force step 7 of the RegExp constructor.
            return Reflect.get(obj, prop);
        },
    });
    const result = splitFn.call(proxy, "splitme");
    // With Symbol.match=undefined, the constructor falls into step 7 (IsRegExp false) → step 8,
    // which does ToString(rx). The proxy's toString returns "/it/" so the splitter pattern is
    // "\/it\/" — the literal characters, which don't match "splitme".
    shouldBe(result, ["splitme"], "proxy with Symbol.match=undefined falls into ToString path");
    if (!accesses.includes("source"))
        throw new Error("expected source to be read on proxy without Symbol.match");
}

// 5. Plain Object proxy WITH Symbol.match passthrough -> step 7 keeps `patternIsRegExp` true
//    but step 6 still does not apply (no [[RegExpMatcher]]). source IS read for construction.
{
    const accesses = [];
    const inner = /it/;
    const proxy = new Proxy(inner, {
        get(obj, prop) {
            accesses.push(prop.toString());
            return Reflect.get(obj, prop);
        },
    });
    const result = splitFn.call(proxy, "splitme");
    shouldBe(result, ["spl","me"], "proxy with passthrough still uses real source");
    if (!accesses.includes("source"))
        throw new Error("expected source to be read on plain-object proxy");
}

// 6. typeof lastIndex !== "number" — the old JS guard refused the fast path here.
//    The C++ split path neither reads nor writes the receiver's lastIndex (the spec uses
//    the splitter), so this should be safe and produce the standard result. Verify both
//    primordial and slow-path receivers behave identically.
{
    const re = /,/;
    re.lastIndex = "not a number";
    shouldBe(splitFn.call(re, "a,b,c"), ["a","b","c"], "non-number lastIndex (primordial structure)");
    if (typeof re.lastIndex !== "string")
        throw new Error("split must not mutate the receiver's lastIndex");
}

// 7. Frozen lastIndex (non-writable, non-number) — receiver structure changes when the
//    attribute changes, so the C++ fast path bails. Slow path constructs a fresh splitter
//    whose lastIndex is writable, so the spec succeeds.
{
    const re = /,/;
    Object.defineProperty(re, "lastIndex", { value: "frozen", writable: false, configurable: false });
    shouldBe(splitFn.call(re, "a,b,c"), ["a","b","c"], "non-writable lastIndex");
    if (re.lastIndex !== "frozen")
        throw new Error("frozen lastIndex was mutated");
}
