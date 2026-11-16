//@ runDefault

// Two instances built the same way share one const impl through the cache, but
// must stay independent to JS: same input formats the same, resolvedOptions()
// returns fresh mutable objects, and mutating one must not perturb the shared
// impl or the other instance.

function expect(label, got, want)
{
    if (got !== want)
        throw new Error(`${label}: expected "${want}", got "${got}"`);
}

const A = new Intl.DateTimeFormat("en-US");
const B = new Intl.DateTimeFormat("en-US");
if (A === B)
    throw new Error("two constructions returned the same JS object");

const ts1 = 0, ts2 = 1577836800000; // 1970-01-01 and 2020-01-01, both UTC.

// Same input formats identically; interleaving B must not perturb A.
expect("A.format(ts1) == B.format(ts1)", A.format(ts1), B.format(ts1));
const aBefore = A.format(ts1);
B.format(ts2);
expect("A.format(ts1) stable under interleaved B.format()", A.format(ts1), aBefore);

// resolvedOptions() returns independent objects; mutating one poisons nothing.
const oa = A.resolvedOptions();
const ob = B.resolvedOptions();
if (oa === ob)
    throw new Error("resolvedOptions() returned the same JS object for two instances");
oa.locale = "MUTATED";
oa.timeZone = "MUTATED";
expect("mutating A's options doesn't affect B", ob.locale, "en-US");
expect("mutating A's options doesn't poison the shared impl (A re-query)",
    A.resolvedOptions().locale, "en-US");
expect("mutating A's options doesn't poison the shared impl (B re-query)",
    B.resolvedOptions().locale, "en-US");
