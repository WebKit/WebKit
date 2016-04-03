function mathTruncOnIntegers(value)
{
    return Math.trunc(value);
}
noInline(mathTruncOnIntegers);

function mathTruncOnDoubles(value)
{
    return Math.trunc(value);
}
noInline(mathTruncOnDoubles);

function mathTruncOnBooleans(value)
{
    return Math.trunc(value);
}
noInline(mathTruncOnBooleans);

// The trivial cases first.
for (var i = 1; i < 1e4; ++i) {
    var truncedValue = mathTruncOnIntegers(i);
    if (truncedValue !== i)
        throw new Error("mathTruncOnIntegers(" + i + ") = " + truncedValue);

    var truncedValue = mathTruncOnIntegers(-i);
    if (truncedValue !== -i)
        throw new Error("mathTruncOnIntegers(" + -i + ") = " + truncedValue);

    var doubleLow = i + 0.4;
    var truncedValue = mathTruncOnDoubles(doubleLow);
    if (truncedValue !== i)
        throw new Error("mathTruncOnDoubles(" + doubleLow + ") = " + truncedValue);

    var doubleHigh = i + 0.6;
    var truncedValue = mathTruncOnDoubles(doubleHigh);
    if (truncedValue !== i)
        throw new Error("mathTruncOnDoubles(" + doubleHigh + ") = " + truncedValue);

    var doubleMid = i + 0.5;
    var truncedValue = mathTruncOnDoubles(doubleMid);
    if (truncedValue !== i)
        throw new Error("mathTruncOnDoubles(" + doubleMid + ") = " + truncedValue);

    var truncedValue = mathTruncOnDoubles(-0.6);
    if (truncedValue !== -0.0)
        throw new Error("mathTruncOnDoubles(-0.6) = " + truncedValue);
}

// Some more interesting cases, some of them well OSR exit when the return value is zero.
for (var i = 0; i < 1e4; ++i) {
    var truncedValue = mathTruncOnIntegers(i);
    if (truncedValue !== i)
        throw new Error("mathTruncOnIntegers(" + i + ") = " + truncedValue);

    var truncedValue = mathTruncOnIntegers(-i);
    if (truncedValue !== -i)
        throw new Error("mathTruncOnIntegers(-" + i + ") = " + truncedValue);

    var truncedValue = mathTruncOnDoubles(-0.4);
    if (truncedValue !== -0.0)
        throw new Error("mathTruncOnDoubles(-0.4) = " + truncedValue);

    var truncedValue = mathTruncOnDoubles(-0.5);
    if (truncedValue !== -0.0)
        throw new Error("mathTruncOnDoubles(-0.5) = " + truncedValue);

    var truncedValue = mathTruncOnDoubles(-0);
    if (!(truncedValue === 0 && (1/truncedValue) === -Infinity))
        throw new Error("mathTruncOnDoubles(-0) = " + truncedValue);

    var truncedValue = mathTruncOnDoubles(NaN);
    if (truncedValue === truncedValue)
        throw new Error("mathTruncOnDoubles(NaN) = " + truncedValue);

    var truncedValue = mathTruncOnDoubles(Number.POSITIVE_INFINITY);
    if (truncedValue !== Number.POSITIVE_INFINITY)
        throw new Error("mathTruncOnDoubles(Number.POSITIVE_INFINITY) = " + truncedValue);

    var truncedValue = mathTruncOnDoubles(Number.NEGATIVE_INFINITY);
    if (truncedValue !== Number.NEGATIVE_INFINITY)
        throw new Error("mathTruncOnDoubles(Number.NEGATIVE_INFINITY) = " + truncedValue);

    var boolean = !!(i % 2);
    var truncedBoolean = mathTruncOnBooleans(boolean);
    if (truncedBoolean != boolean)
        throw new Error("mathTruncOnDoubles(" + boolean + ") = " + truncedBoolean);
}

function uselessMathTrunc(value)
{
    return Math.trunc(value|0);
}
noInline(uselessMathTrunc);

for (var i = 0; i < 1e4; ++i) {
    var truncedValue = uselessMathTrunc(i);
    if (truncedValue !== i)
        throw new Error("uselessMathTrunc(" + i + ") = " + truncedValue);

    var doubleLow = i + 0.4;
    var truncedValue = uselessMathTrunc(doubleLow);
    if (truncedValue !== i)
        throw new Error("uselessMathTrunc(" + doubleLow + ") = " + truncedValue);

    var doubleHigh = i + 0.6;
    var truncedValue = uselessMathTrunc(doubleHigh);
    if (truncedValue !== i)
        throw new Error("uselessMathTrunc(" + doubleHigh + ") = " + truncedValue);

    var doubleMid = i + 0.5;
    var truncedValue = uselessMathTrunc(doubleMid);
    if (truncedValue !== i)
        throw new Error("uselessMathTrunc(" + doubleMid + ") = " + truncedValue);

    var truncedValue = uselessMathTrunc(-0.4);
    if (truncedValue !== 0)
        throw new Error("uselessMathTrunc(-0.4) = " + truncedValue);

    var truncedValue = uselessMathTrunc(-0.5);
    if (truncedValue !== 0)
        throw new Error("uselessMathTrunc(-0.5) = " + truncedValue);

    var truncedValue = uselessMathTrunc(-0.6);
    if (truncedValue !== 0)
        throw new Error("uselessMathTrunc(-0.6) = " + truncedValue);
}

function mathTruncWithOverflow(value)
{
    return Math.trunc(value);
}
noInline(mathTruncWithOverflow);

for (var i = 0; i < 1e4; ++i) {
    var bigValue = 1000000000000;
    var truncedValue = mathTruncWithOverflow(bigValue);
    if (truncedValue !== bigValue)
        throw new Error("mathTruncWithOverflow(" + bigValue + ") = " + truncedValue);
}

function mathTruncConsumedAsDouble(value)
{
    return Math.trunc(value) * 0.5;
}
noInline(mathTruncConsumedAsDouble);

for (var i = 0; i < 1e4; ++i) {
    var doubleValue = i + 0.1;
    var truncedValue = mathTruncConsumedAsDouble(doubleValue);
    if (truncedValue !== (i * 0.5))
        throw new Error("mathTruncConsumedAsDouble(" + doubleValue + ") = " + truncedValue);

    var doubleValue = i + 0.6;
    var truncedValue = mathTruncConsumedAsDouble(doubleValue);
    if (truncedValue !== (i * 0.5))
        throw new Error("mathTruncConsumedAsDouble(" + doubleValue + ") = " + truncedValue);

}

function mathTruncDoesNotCareAboutMinusZero(value)
{
    return Math.trunc(value)|0;
}
noInline(mathTruncDoesNotCareAboutMinusZero);

for (var i = 0; i < 1e4; ++i) {
    var doubleMid = i + 0.5;
    var truncedValue = mathTruncDoesNotCareAboutMinusZero(doubleMid);
    if (truncedValue !== i)
        throw new Error("mathTruncDoesNotCareAboutMinusZero(" + doubleMid + ") = " + truncedValue);
}


// *** Function arguments. ***
function mathTruncNoArguments()
{
    return Math.trunc();
}
noInline(mathTruncNoArguments);

function mathTruncTooManyArguments(a, b, c)
{
    return Math.trunc(a, b, c);
}
noInline(mathTruncTooManyArguments);

for (var i = 0; i < 1e4; ++i) {
    var value = mathTruncNoArguments();
    if (value === value)
        throw new Error("mathTruncNoArguments() = " + value);

    var value = mathTruncTooManyArguments(2.1, 3, 5);
    if (value !== 2)
        throw new Error("mathTruncTooManyArguments() = " + value);
}


// *** Constant as arguments. ***
function testMathTruncOnConstants()
{
    var value = Math.trunc(0);
    if (value !== 0)
        throw new Error("Math.trunc(0) = " + value);
    var value = Math.trunc(-0);
    if (!(value === 0 && (1/value) === -Infinity))
        throw new Error("Math.trunc(-0) = " + value);
    var value = Math.trunc(1);
    if (value !== 1)
        throw new Error("Math.trunc(1) = " + value);
    var value = Math.trunc(-1);
    if (value !== -1)
        throw new Error("Math.trunc(-1) = " + value);
    var value = Math.trunc(42);
    if (value !== 42)
        throw new Error("Math.trunc(42) = " + value);
    var value = Math.trunc(-42.2);
    if (value !== -42)
        throw new Error("Math.trunc(-42.2) = " + value);
    var value = Math.trunc(NaN);
    if (value === value)
        throw new Error("Math.trunc(NaN) = " + value);
    var value = Math.trunc(Number.POSITIVE_INFINITI);
    if (value === value)
        throw new Error("Math.trunc(Number.POSITIVE_INFINITI) = " + value);
    var value = Math.trunc(Number.NEGATIVE_INFINITI);
    if (value === value)
        throw new Error("Math.trunc(Number.NEGATIVE_INFINITI) = " + value);
    var value = Math.trunc(Math.E);
    if (value !== 2)
        throw new Error("Math.trunc(Math.E) = " + value);
}
noInline(testMathTruncOnConstants);

for (var i = 0; i < 1e4; ++i) {
    testMathTruncOnConstants();
}


// *** Struct transition. ***
function mathTruncStructTransition(value)
{
    return Math.trunc(value);
}
noInline(mathTruncStructTransition);

for (var i = 0; i < 1e4; ++i) {
    var value = mathTruncStructTransition(42.5);
    if (value !== 42)
        throw new Error("mathTruncStructTransition(42.5) = " + value);
}

Math.trunc = function() { return arguments[0] + 5; }

var value = mathTruncStructTransition(42);
if (value !== 47)
    throw new Error("mathTruncStructTransition(42) after transition = " + value);
