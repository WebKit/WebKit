// Original test case.
function isNaNOnDouble(value)
{
    return (+value) !== value;
}
noInline(isNaNOnDouble);

function testIsNaNOnDoubles()
{
    var value = isNaNOnDouble(-0);
    if (value)
        throw "isNaNOnDouble(-0) = " + value;

    var value = isNaNOnDouble(NaN);
    if (!value)
        throw "isNaNOnDouble(NaN) = " + value;

    var value = isNaNOnDouble(Number.POSITIVE_INFINITY);
    if (value)
        throw "isNaNOnDouble(Number.POSITIVE_INFINITY) = " + value;
}
noInline(testIsNaNOnDoubles);

for (let i = 0; i < 1e6; ++i) {
    testIsNaNOnDoubles();
}

// Simplified test case.
function isNaNOnDouble2(value)
{
    let valueToNumber = (+value);
    return valueToNumber !== valueToNumber;
}
noInline(isNaNOnDouble2);

// Warm up without NaN.
for (let i = 0; i < 1e6; ++i) {
    if (isNaNOnDouble2(1.5))
        throw "Failed isNaNOnDouble(1.5)";
}

// Then pass some NaNs.
for (let i = 0; i < 1e6; ++i) {
    if (!isNaNOnDouble2(NaN))
        throw "Failed isNaNOnDouble(NaN)";
}
