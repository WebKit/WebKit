function mathTruncInt(i)
{
    return Math.trunc(i);
}
noInline(mathTruncInt);

for (var i = 0; i < 1e5; ++i)
    mathTruncInt(i);

function mathTruncDouble(i)
{
    return Math.trunc(i);
}
noInline(mathTruncDouble);

for (var i = 0; i < 1e5; ++i)
    mathTruncDouble(i * 0.6);

function mathTruncMixed(i)
{
    return Math.trunc(i);
}
noInline(mathTruncMixed);

for (var i = 0; i < 1e5; ++i) {
    if (i % 2 === 0)
        mathTruncDouble(i * 0.6);
    else
        mathTruncDouble(i);
}

function mathTruncDoubleDoesNotCareNegativeZero(i)
{
    return Math.trunc(i) | 0;
}
noInline(mathTruncDoubleDoesNotCareNegativeZero);

for (var i = 0; i < 1e5; ++i)
    mathTruncDoubleDoesNotCareNegativeZero(i * 0.6);
