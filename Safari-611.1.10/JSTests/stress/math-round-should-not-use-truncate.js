function mathRoundDoesNotCareAboutMinusZero(value)
{
    return Math.round(value);
}
noInline(mathRoundDoesNotCareAboutMinusZero);

for (var i = 0; i < 1e4; ++i) {
    var doubleMid = -9901 - 0.6;
    var roundedValue = mathRoundDoesNotCareAboutMinusZero(doubleMid);
    if (roundedValue !== -9902)
        throw "mathRoundDoesNotCareAboutMinusZero(" + doubleMid + ") = " + roundedValue;
}
