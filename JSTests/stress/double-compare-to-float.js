function canSimplifyToFloat(a, b)
{
    return Math.fround(a) === Math.fround(b);
}
noInline(canSimplifyToFloat);

function canSimplifyToFloatWithConstant(a)
{
    return Math.fround(a) === 1.0;
}
noInline(canSimplifyToFloatWithConstant);

function cannotSimplifyA(a, b)
{
    return a === Math.fround(b);
}
noInline(cannotSimplifyA);

function cannotSimplifyB(a, b)
{
    return Math.fround(a) === b;
}
noInline(cannotSimplifyB);

for (let i = 1; i < 1e4; ++i) {
    if (canSimplifyToFloat(Math.PI, Math.PI) !== true)
        throw "Failed canSimplifyToFloat(Math.PI, Math.PI)";
    if (canSimplifyToFloat(Math.LN2, Math.PI) !== false)
        throw "Failed canSimplifyToFloat(Math.LN2, Math.PI)";

    if (canSimplifyToFloatWithConstant(Math.PI) !== false)
        throw "Failed canSimplifyToFloatWithConstant(Math.PI)";
    if (canSimplifyToFloatWithConstant(1) !== true)
        throw "Failed canSimplifyToFloatWithConstant(1)";

    if (cannotSimplifyA(Math.PI, Math.PI) !== false)
        throw "Failed cannotSimplifyA(Math.PI, Math.PI)";
    if (cannotSimplifyA(Math.fround(Math.PI), Math.PI) !== true)
        throw "Failed cannotSimplifyA(Math.round(Math.PI), Math.PI)";
    if (cannotSimplifyA(Math.LN2, Math.PI) !== false)
        throw "Failed cannotSimplifyA(Math.LN2, Math.PI)";

    if (cannotSimplifyB(Math.PI, Math.PI) !== false)
        throw "Failed cannotSimplifyA(Math.PI, Math.PI)";
    if (cannotSimplifyB(Math.PI, Math.fround(Math.PI)) !== true)
        throw "Failed cannotSimplifyA(Math.round(Math.PI), Math.PI)";
    if (cannotSimplifyB(Math.LN2, Math.PI) !== false)
        throw "Failed cannotSimplifyA(Math.LN2, Math.PI)";
}
