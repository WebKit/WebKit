//@ requireOptions("--useDollarVM=1")
// Arguments-elimination "mixed" coverage for fused spread calls (op_call_varargs_with_spread and its
// LoadVarargsWithSpread/VarargsLengthWithSpread siblings). escapeSpreadOperands escapes each operand
// independently (no all-or-nothing, no fixpoint), so a single fused call can carry any mix of:
//   * a forwarded rest        (PhantomCreateRest  -> copied from the caller frame via the *Forward ops),
//   * a constant buffer spread(PhantomNewArrayBuffer -> stored as its constant butterfly cell),
//   * a materialized spread   (a real heap array),
//   * plain literals.
// The *WithSpread backends (DFG SpeculativeJIT + FTL) must expand every combination correctly.

function sum(...received) {
    let total = 0;
    for (let i = 0; i < received.length; i++)
        total += received[i];
    return total;
}
noInline(sum);

// Forwarded rest with a trailing literal: rest is eliminated (PhantomCreateRest) and forwarded.
function restThenLiteral(...rest) { return sum(...rest, 42); }
noInline(restThenLiteral);

// Leading literal then forwarded rest: the forwarded segment does not start at argument 0.
function literalThenRest(...rest) { return sum(42, ...rest); }
noInline(literalThenRest);

// Literal + forwarded rest + constant buffer: forwarded segment between other segments.
function literalRestBuffer(...rest) { return sum(7, ...rest, ...[100, 200]); }
noInline(literalRestBuffer);

// Materialized (escaping) array spread alongside a constant buffer spread.
let sink;
function materializedAndBuffer(externalArray) {
    let r = sum(...externalArray, ...[1000, 2000]);
    sink = function () { return externalArray; };
    return r;
}
noInline(materializedAndBuffer);

// Forwarded rest alongside a materialized array spread (whatever AE decides for the rest, the result must
// be correct).
function restAndMaterialized(externalArray, ...rest) { return sum(...rest, ...externalArray); }
noInline(restAndMaterialized);

// A global array spreads as a materialized operand without interfering with the caller frame's arguments, so
// the rest genuinely stays eliminated (PhantomCreateRest) *next to* a materialized spread — the true mixed
// case that routes through the *WithSpread backends' forwarded-segment path rather than the all-forwarded
// fast path.
var globalArray = [100, 200];
var bigGlobal = [];
let bigGlobalSum = 0;
for (let i = 0; i < 40; i++) { bigGlobal.push(i); bigGlobalSum += i; }
function restThenGlobal(...rest) { return sum(...rest, ...globalArray); }
noInline(restThenGlobal);
function globalThenRest(...rest) { return sum(...globalArray, ...rest); }
noInline(globalThenRest);
function restGlobalBuffer(...rest) { return sum(...rest, ...globalArray, ...[7, 8]); }
noInline(restGlobalBuffer);
function restAndBigGlobal(...rest) { return sum(...rest, ...bigGlobal); }
noInline(restAndBigGlobal);

function ck(actual, expected, label) {
    if (actual !== expected)
        throw new Error(`${label}: expected ${expected} got ${actual}`);
}

const ext = [100, 200];
const big = [];
let bigSum = 0;
for (let i = 0; i < 40; i++) { big.push(i); bigSum += i; }

for (let i = 0; i < testLoopCount; i++) {
    ck(restThenLiteral(1, 2, 3), 6 + 42, "restThenLiteral");
    ck(restThenLiteral(), 42, "restThenLiteral-empty");
    ck(restThenLiteral(5, 5, 5, 5, 5), 25 + 42, "restThenLiteral-long");
    ck(literalThenRest(1, 2, 3), 42 + 6, "literalThenRest");
    ck(literalThenRest(), 42, "literalThenRest-empty");
    ck(literalRestBuffer(1, 2, 3), 7 + 6 + 300, "literalRestBuffer");
    ck(literalRestBuffer(), 7 + 300, "literalRestBuffer-empty");
    ck(materializedAndBuffer(ext), 300 + 3000, "materializedAndBuffer");
    ck(restAndMaterialized(ext, 1, 2, 3), 6 + 300, "restAndMaterialized");
    ck(restAndMaterialized(ext), 300, "restAndMaterialized-empty-rest");
    ck(restAndMaterialized(big, 1, 2), 3 + bigSum, "restAndMaterialized-big");
    // True mixed: eliminated forwarded rest + materialized (global) spread in one fused call.
    ck(restThenGlobal(1, 2, 3), 6 + 300, "restThenGlobal");
    ck(restThenGlobal(), 300, "restThenGlobal-empty");
    ck(restThenGlobal(1, 2, 3, 4, 5, 6), 21 + 300, "restThenGlobal-long");
    ck(globalThenRest(1, 2, 3), 300 + 6, "globalThenRest");
    ck(restGlobalBuffer(1, 2), 3 + 300 + 15, "restGlobalBuffer");
    ck(restAndBigGlobal(1, 2), 3 + bigGlobalSum, "restAndBigGlobal");
}
