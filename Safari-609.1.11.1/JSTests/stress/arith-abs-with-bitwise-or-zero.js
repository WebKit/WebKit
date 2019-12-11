function opaqueAbs(value)
{
    return Math.abs(value)|0;
}
noInline(opaqueAbs);

for (let i = 0; i < 1e4; ++i) {
    var positiveResult = opaqueAbs(i);
    if (positiveResult !== i)
        throw "Incorrect result at i = " + i + " result = " + positiveResult;
    var negativeResult = opaqueAbs(-i);
    if (negativeResult !== i)
        throw "Incorrect result at -i = " + -i + " result = " + negativeResult;
}


var intMax = 2147483647;
var intMin = 2147483647;

var intMaxResult = opaqueAbs(intMax);
if (intMaxResult !== intMax)
    throw "Incorrect result at intMax result = " + intMaxResult;
var intMaxResult = opaqueAbs(intMin);
if (intMaxResult !== intMin)
    throw "Incorrect result at intMax result = " + intMaxResult;

// Numbers around IntMax/IntMin. Numbers outside the bounds are doubles and opaqueAbs()
// has to OSR Exit to handle them correctly.
for (let i = intMax - 1e4; i < intMax + 1e4; ++i) {
    var positiveResult = opaqueAbs(i);
    if (positiveResult !== (i|0))
        throw "Incorrect result at i = " + i + " result = " + positiveResult;
    var negativeResult = opaqueAbs(-i);
    if (negativeResult !== (i|0))
        throw "Incorrect result at -i = " + -i + " result = " + negativeResult;
}

// Edge cases and exits.
if (opaqueAbs(NaN) !== 0)
    throw "opaqueAbs(NaN) failed.";
if (opaqueAbs(Infinity) !== 0)
    throw "opaqueAbs(Infinity) failed.";
if (opaqueAbs(-Infinity) !== 0)
    throw "opaqueAbs(-Infinity) failed.";
if (opaqueAbs(null) !== 0)
    throw "opaqueAbs(null) failed.";
if (opaqueAbs(undefined) !== 0)
    throw "opaqueAbs(undefined) failed.";
if (opaqueAbs(true) !== 1)
    throw "opaqueAbs(true) failed.";
if (opaqueAbs(false) !== 0)
    throw "opaqueAbs(false) failed.";
if (opaqueAbs({foo:"bar"}) !== 0)
    throw "opaqueAbs({foo:'bar'}) failed.";
