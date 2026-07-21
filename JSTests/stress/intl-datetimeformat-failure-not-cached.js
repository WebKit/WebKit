// Failed constructions must not be cached as successes: repeated calls with the
// same bad input must keep throwing, and a good call interleaved with bad ones
// must keep its own answer.

function expect(label, got, want)
{
    if (got !== want)
        throw new Error(`${label}: expected ${JSON.stringify(want)}, got ${JSON.stringify(got)}`);
}

function expectThrows(label, fn)
{
    let threw = null;
    try { fn(); } catch (e) { threw = e; }
    if (!threw)
        throw new Error(`${label}: expected to throw, returned normally`);
    return threw;
}

// (1) Invalid language tag must throw on every call — RangeError per spec.
const e1 = expectThrows("first call on invalid tag", () => new Intl.DateTimeFormat("en-INVALID-TAG-?"));
const e2 = expectThrows("second call on invalid tag", () => new Intl.DateTimeFormat("en-INVALID-TAG-?"));
const e3 = expectThrows("third call on invalid tag", () => new Intl.DateTimeFormat("en-INVALID-TAG-?"));

expect("first throw is RangeError", e1 instanceof RangeError, true);
expect("second throw is RangeError", e2 instanceof RangeError, true);
expect("third throw is RangeError", e3 instanceof RangeError, true);

// (2) Successful construction must not be poisoned by interleaved failures.
const goodA = new Intl.DateTimeFormat("en-US");
expectThrows("interleaved bad call 1", () => new Intl.DateTimeFormat("xx-INVALID-?"));
const goodB = new Intl.DateTimeFormat("en-US");
expectThrows("interleaved bad call 2", () => new Intl.DateTimeFormat("xx-INVALID-?"));
const goodC = new Intl.DateTimeFormat("en-US");

expect("good resolution stable under interleaved failures (A vs B)",
    goodA.resolvedOptions().locale, goodB.resolvedOptions().locale);
expect("good resolution stable under interleaved failures (A vs C)",
    goodA.resolvedOptions().locale, goodC.resolvedOptions().locale);
expect("good format stable under interleaved failures",
    goodA.format(0), goodC.format(0));

// (3) Stress: many repeated bad calls must all throw.
for (let i = 0; i < 50; ++i)
    expectThrows(`stress bad call ${i}`, () => new Intl.DateTimeFormat("zz-NOT-A-TAG"));

// (4) Good calls must keep working after a stress of bad ones.
const goodAfter = new Intl.DateTimeFormat("en-US");
expect("good resolution survives stress of bad calls",
    goodAfter.resolvedOptions().locale, goodA.resolvedOptions().locale);
expect("good format survives stress of bad calls",
    goodAfter.format(0), goodA.format(0));

// (5) A valid locale sharing a prefix with a failing one must still resolve.
expectThrows("bad ja-INVALID", () => new Intl.DateTimeFormat("ja-INVALID-?"));
const goodJa = new Intl.DateTimeFormat("ja");
expect("ja resolves after sibling failure",
    goodJa.resolvedOptions().locale.startsWith("ja"), true);
