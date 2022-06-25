function mathTrunc(value)
{
    return Math.trunc(value);
}
noInline(mathTrunc);

for (var i = 0; i < 1e4; ++i) {
    var doubleMid = -9901 - 0.6;
    var roundedValue = mathTrunc(doubleMid);
    if (roundedValue !== -9901)
        throw "mathRoundDoesNotCareAboutMinusZero(" + doubleMid + ") = " + roundedValue;
}
