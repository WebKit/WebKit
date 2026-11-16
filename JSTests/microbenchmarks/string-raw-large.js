//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
// String.raw over a large tagged template: 33 raw segments + 32 substitutions.
// A tagged template's raw array is spec-frozen (ArrayWithSlowPutArrayStorage with
// its elements in a SparseArrayValueMap), so this exercises String.raw's frozen-array
// segment reads together with many-fiber rope building. The substitution varies each
// call (so the result can't be hoisted) but stays a single digit, keeping the result
// length constant for a cheap correctness check.
function test(x) {
    return String.raw`s0${x}s1${x}s2${x}s3${x}s4${x}s5${x}s6${x}s7${x}s8${x}s9${x}s10${x}s11${x}s12${x}s13${x}s14${x}s15${x}s16${x}s17${x}s18${x}s19${x}s20${x}s21${x}s22${x}s23${x}s24${x}s25${x}s26${x}s27${x}s28${x}s29${x}s30${x}s31${x}s32`.length;
}
noInline(test);

var expected = test(0);
var total = 0;
for (var i = 0; i < 3e5; ++i)
    total += test(i % 10);

if (total !== expected * 3e5)
    throw "Error: bad result: " + total;
