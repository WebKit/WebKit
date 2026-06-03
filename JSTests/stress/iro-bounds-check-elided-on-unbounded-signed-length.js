//@ skip if !$isFTLPlatform
//@ runDefault("--useTestingHelpers=1", "--useDollarVM=1", "--useConcurrentJIT=0", "--useLoopUnrolling=0", "--useOSREntryToDFG=0")

// Signed-condition typed-array loop: `i < arr.length` with arr a typed
// array (length capped at INT32_MAX by JSC). The body indexes arr[i].
//
// IRO elides the bounds check on arr[i] because:
//   - CompareLess(i, length) taken gives `i < length` on the body entry.
//   - The typed-array length canonicalization (run inside IRO) lets the
//     fact propagate through the loop's i-Phi to the bounds check.
//   - `i >= 0` falls out of the back-edge analysis combined with the
//     entry value (i = 0).
//
// Soundness: for any Int32Array, length is in [0, INT32_MAX]. Starting
// at i = 0 and incrementing by 1, i in the body is always in
// [0, length - 1]. Even when length = INT32_MAX, the loop exits at
// i = INT32_MAX (signed `i < INT32_MAX` is false), so `(i + 1) | 0`
// never wraps in the body. The dead self-assignment in the body is
// proved dead by IRO and doesn't change the conclusion.

load("./resources/iro-test-helpers.js", "caller relative");

function fn(arr, arg) {
    let sum = 0|0;
    for (let i = 0; i < arr.length; i = (i + 1)|0) {
        if (arg === 1337 && i === -1)
            i = -1;
        sum = ((sum|0) + (arr[i]|0))|0;
    }
    return sum;
}
noInline(fn);

const arr = new Int32Array(8);
for (let i = 0; i < arr.length; ++i) arr[i] = i + 1;

const expectedFull = 1 + 2 + 3 + 4 + 5 + 6 + 7 + 8;
for (let i = 0; i < testLoopCount; ++i) {
    const r = fn(arr, 1337);
    if (r !== expectedFull)
        throw new Error("warmup mismatch (Int32Array, arg=1337): got " + r
            + ", expected " + expectedFull);
}

const iro = makeIROHelper(fn);

if (iro.opCount("GetArrayLength") > 0)
    throw new Error("expected a typed-array length node, got GetArrayLength — "
        + "this test no longer probes the no-cap path");
if (iro.opCount("GetUndetachedTypeArrayLength") + iro.opCount("GetTypedArrayLengthAsInt52") === 0)
    throw new Error("expected GetUndetachedTypeArrayLength or "
        + "GetTypedArrayLengthAsInt52 in the IR");

const checkBounds = iro.opCount("CheckInBounds") + iro.opCount("CheckInBoundsInt52");
if (checkBounds !== 0)
    throw new Error("expected IRO to eliminate the bounds check; "
        + checkBounds + " CheckInBounds node(s) remain");
for (const line of iro.dfgGraph.split("\n")) {
    if (!/\b(GetByVal|EnumeratorGetByVal)\(/.test(line)) continue;
    if (!line.includes("InBounds"))
        throw new Error("expected GetByVal in InBounds array mode after "
            + "bounds-check elimination, got: " + line);
}

const empty = new Int32Array(0);
if (fn(empty, 1337) !== 0)
    throw new Error("fn(empty, 1337) wrong: " + fn(empty, 1337));
if (fn(empty, 999) !== 0)
    throw new Error("fn(empty, 999) wrong: " + fn(empty, 999));

const one = new Int32Array([42]);
if (fn(one, 1337) !== 42)
    throw new Error("fn(one, 1337) wrong: " + fn(one, 1337));
if (fn(one, 999) !== 42)
    throw new Error("fn(one, 999) wrong: " + fn(one, 999));

if (fn(arr, 999) !== expectedFull)
    throw new Error("fn(arr, 999) wrong: " + fn(arr, 999));

print("PASS");
