//@ skip if !$isFTLPlatform
//@ runDefault("--useTestingHelpers=1", "--useDollarVM=1", "--useConcurrentJIT=0", "--useLoopUnrolling=0")

// Exercises range(id) and rangeOf(atId, ofId): the value range IRO proves for a
// probe, and the range of one probe as seen at another probe's IR position.
// Nested loops give two induction variables with constant bounds, so IRO proves
// exact ranges; both are probed at the inner point, so the outer variable also
// shows up in the inner probe's per-anchor ranges.

load("./resources/iro-test-helpers.js", "caller relative");

function fn() {
    let sum = 0;
    for (let i = 0; i < 10; i++) {
        for (let j = 0; j < 5; j++) {
            const pi = $vm.probe("i", i);
            const pj = $vm.probe("j", j);
            sum += pi + pj;
        }
    }
    return sum;
}
noInline(fn);

for (let k = 0; k < testLoopCount; k++) fn();
const iro = makeIROHelper(fn);

function expectRange(r, min, max, what) {
    if (!r || r.min !== min || r.max !== max)
        throw new Error(what + ": expected " + min + ".." + max
            + ", got " + (r ? r.min + ".." + r.max : "null"));
}

expectRange(iro.range("i"), 0, 9, 'range("i")');
expectRange(iro.range("j"), 0, 4, 'range("j")');

// At "j"'s anchor (inner loop), the outer induction variable "i" is in scope,
// so its range is visible there.
expectRange(iro.rangeOf("j", "i"), 0, 9, 'rangeOf("j","i")');

// rangeOf(id, id) is the probe's own range.
expectRange(iro.rangeOf("i", "i"), 0, 9, 'rangeOf("i","i")');
