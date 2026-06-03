//@ skip if !$isFTLPlatform
//@ runDefault("--useTestingHelpers=1", "--useDollarVM=1", "--useConcurrentJIT=0", "--useLoopUnrolling=0", "--useOSREntryToDFG=0")

// Companion to iro-bounds-check-elided-on-unbounded-length.js. That test
// pins down the signed-condition case. This one covers the unsigned
// variant: `(i>>>0) < (arr.length>>>0)`.
//
// IRO's CompareBelow taken-edge extraction emits both halves of
// `a <u b ∧ 0 <=s b => 0 <=s a <s b`: `a < b` and `a >= 0`. When that
// pair propagates from the back-edge value through the i-Phi, IRO has
// enough to elide the bounds check on arr[i].
//
// Soundness argument for the elision: for any Int32Array, length is in
// [0, INT32_MAX]. Starting at i = 0 and incrementing by 1, i in the
// body is always in [0, length - 1], so i never wraps. Even
// hypothetically, if i wrapped to negative, (i>>>0) would be >= 2^31
// > length, so the loop would exit before the body saw a negative i.
// We can't allocate an Int32Array close to INT32_MAX, so we drive the
// loop boundary via empty / single / multi-element sizes.

load("./resources/iro-test-helpers.js", "caller relative");

function fn(arr) {
    let sum = 0|0;
    for (let i = 0; (i>>>0) < (arr.length>>>0); i = (i + 1)|0) {
        sum = ((sum|0) + (arr[i]|0))|0;
    }
    return sum;
}
noInline(fn);

const arr = new Int32Array(8);
for (let i = 0; i < arr.length; ++i) arr[i] = i + 1;
const expected = 1 + 2 + 3 + 4 + 5 + 6 + 7 + 8;

for (let i = 0; i < testLoopCount; ++i) {
    const r = fn(arr);
    if (r !== expected)
        throw new Error("warmup mismatch: got " + r + " expected " + expected);
}

const iro = makeIROHelper(fn);

// Confirm we're on the no-cap length path.
if (iro.opCount("GetArrayLength") > 0)
    throw new Error("expected GetUndetachedTypeArrayLength (no static cap), "
        + "got GetArrayLength");
if (iro.opCount("GetUndetachedTypeArrayLength") + iro.opCount("GetTypedArrayLengthAsInt52") === 0)
    throw new Error("expected a typed-array length node in the IR");

// Confirm IRO eliminated the bounds check on the typed-array GetByVal.
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

// Runtime soundness across array sizes.
if (fn(new Int32Array(0)) !== 0)
    throw new Error("fn(Int32Array(0)) wrong");

const one = new Int32Array([42]);
if (fn(one) !== 42)
    throw new Error("fn(Int32Array([42])) wrong: " + fn(one));

const sixteen = new Int32Array(16);
for (let i = 0; i < sixteen.length; ++i) sixteen[i] = i + 1;
if (fn(sixteen) !== 136)
    throw new Error("fn(Int32Array(16)) wrong: " + fn(sixteen));

print("PASS");
