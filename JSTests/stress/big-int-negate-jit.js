//@ skip if not $jitTests
//@ runBigIntEnabled

function assert(a, b) {
    if (a !== b)
        throw new Error("Bad!");
}

function negateBigInt(n) {
    return -n;
}
noInline(negateBigInt);

for (let i = 0; i < 100000; i++) {
    assert(negateBigInt(100n), -100n);
    assert(negateBigInt(-0x1fffffffffffff01n), 0x1fffffffffffff01n);
}

if (numberOfDFGCompiles(negateBigInt) > 1)
    throw "Failed negateBigInt(). We should have compiled a single negate for the BigInt type.";

function negateBigIntSpecializedToInt(n) {
    return -n;
}
noInline(negateBigIntSpecializedToInt);

for (let i = 0; i < 100000; i++) {
    negateBigIntSpecializedToInt(100);
}

assert(negateBigIntSpecializedToInt(100n), -100n);

// Testing case mixing int and BigInt speculations
function mixedSpeculationNegateBigInt(n, arr) {
    return -(-(-n));
}
noInline(mixedSpeculationNegateBigInt);

for (let i = 0; i < 100000; i++) {
    if (i % 2)
        assert(mixedSpeculationNegateBigInt(100), -100);
    else
        assert(mixedSpeculationNegateBigInt(-0x1fffffffffffff01n), 0x1fffffffffffff01n);
}

if (numberOfDFGCompiles(mixedSpeculationNegateBigInt) > 1)
    throw "Failed mixedSpeculationNegateBigInt(). We should have compiled a single negate for the BigInt type.";

