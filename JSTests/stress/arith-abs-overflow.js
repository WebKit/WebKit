function opaqueAbs(value)
{
    return Math.abs(value);
}
noInline(opaqueAbs);

// Warmup.
for (let i = 0; i < 1e4; ++i) {
    var positiveResult = opaqueAbs(i);
    if (positiveResult !== i)
        throw "Incorrect positive result at i = " + i + " result = " + positiveResult;
    var negativeResult = opaqueAbs(-i);
    if (negativeResult !== i)
        throw "Incorrect negative result at -i = " + -i + " result = " + negativeResult;
}

// Overflow.
for (let i = 0; i < 1e4; ++i) {
    var overflowResult = opaqueAbs(-2147483648);
    if (overflowResult !== 2147483648)
        throw "Incorrect overflow result at i = " + i + " result = " + overflowResult;
}
