
function mathRoundOnIntegers(value)
{
    return Math.round(value);
}
noInline(mathRoundOnIntegers);

function mathRoundOnDoubles(value)
{
    return Math.round(value);
}
noInline(mathRoundOnDoubles);

function mathRoundOnBooleans(value)
{
    return Math.round(value);
}
noInline(mathRoundOnBooleans);

// The trivial cases first.
for (var i = 1; i < 1e4; ++i) {
    var roundedValue = mathRoundOnIntegers(i);
    if (roundedValue !== i)
        throw "mathRoundOnIntegers(" + i + ") = " + roundedValue;

    var roundedValue = mathRoundOnIntegers(-i);
    if (roundedValue !== -i)
        throw "mathRoundOnIntegers(" + -i + ") = " + roundedValue;

    var doubleLow = i + 0.4;
    var roundedValue = mathRoundOnDoubles(doubleLow);
    if (roundedValue !== i)
        throw "mathRoundOnDoubles(" + doubleLow + ") = " + roundedValue;

    var doubleHigh = i + 0.6;
    var roundedValue = mathRoundOnDoubles(doubleHigh);
    if (roundedValue !== i + 1)
        throw "mathRoundOnDoubles(" + doubleHigh + ") = " + roundedValue;

    var doubleMid = i + 0.5;
    var roundedValue = mathRoundOnDoubles(doubleMid);
    if (roundedValue !== i + 1)
        throw "mathRoundOnDoubles(" + doubleMid + ") = " + roundedValue;

    var roundedValue = mathRoundOnDoubles(-0.6);
    if (roundedValue !== -1)
        throw "mathRoundOnDoubles(-0.6) = " + roundedValue;
}

// Some more interesting cases, some of them well OSR exit when the return value is zero.
for (var i = 0; i < 1e4; ++i) {
    var roundedValue = mathRoundOnIntegers(i);
    if (roundedValue !== i)
        throw "mathRoundOnIntegers(" + i + ") = " + roundedValue;

    var roundedValue = mathRoundOnIntegers(-i);
    if (roundedValue !== -i)
        throw "mathRoundOnIntegers(-" + i + ") = " + roundedValue;

    var roundedValue = mathRoundOnDoubles(-0.4);
    if (roundedValue !== 0)
        throw "mathRoundOnDoubles(-0.4) = " + roundedValue;

    var roundedValue = mathRoundOnDoubles(-0.5);
    if (roundedValue !== 0)
        throw "mathRoundOnDoubles(-0.5) = " + roundedValue;

    var roundedValue = mathRoundOnDoubles(-0);
    if (!(roundedValue === 0 && (1/roundedValue) === -Infinity))
        throw "mathRoundOnDoubles(-0) = " + roundedValue;

    var roundedValue = mathRoundOnDoubles(NaN);
    if (roundedValue === roundedValue)
        throw "mathRoundOnDoubles(NaN) = " + roundedValue;

    var roundedValue = mathRoundOnDoubles(Number.POSITIVE_INFINITY);
    if (roundedValue !== Number.POSITIVE_INFINITY)
        throw "mathRoundOnDoubles(Number.POSITIVE_INFINITY) = " + roundedValue;

    var roundedValue = mathRoundOnDoubles(Number.NEGATIVE_INFINITY);
    if (roundedValue !== Number.NEGATIVE_INFINITY)
        throw "mathRoundOnDoubles(Number.NEGATIVE_INFINITY) = " + roundedValue;

    var boolean = !!(i % 2);
    var roundedBoolean = mathRoundOnBooleans(boolean);
    if (roundedBoolean != boolean)
        throw "mathRoundOnDoubles(" + boolean + ") = " + roundedBoolean;
}

function uselessMathRound(value)
{
    return Math.round(value|0);
}
noInline(uselessMathRound);

for (var i = 0; i < 1e4; ++i) {
    var roundedValue = uselessMathRound(i);
    if (roundedValue !== i)
        throw "uselessMathRound(" + i + ") = " + roundedValue;

    var doubleLow = i + 0.4;
    var roundedValue = uselessMathRound(doubleLow);
    if (roundedValue !== i)
        throw "uselessMathRound(" + doubleLow + ") = " + roundedValue;

    var doubleHigh = i + 0.6;
    var roundedValue = uselessMathRound(doubleHigh);
    if (roundedValue !== i)
        throw "uselessMathRound(" + doubleHigh + ") = " + roundedValue;

    var doubleMid = i + 0.5;
    var roundedValue = uselessMathRound(doubleMid);
    if (roundedValue !== i)
        throw "uselessMathRound(" + doubleMid + ") = " + roundedValue;

    var roundedValue = uselessMathRound(-0.4);
    if (roundedValue !== 0)
        throw "uselessMathRound(-0.4) = " + roundedValue;

    var roundedValue = uselessMathRound(-0.5);
    if (roundedValue !== 0)
        throw "uselessMathRound(-0.5) = " + roundedValue;

    var roundedValue = uselessMathRound(-0.6);
    if (roundedValue !== 0)
        throw "uselessMathRound(-0.6) = " + roundedValue;
}

function mathRoundWithOverflow(value)
{
    return Math.round(value);
}
noInline(mathRoundWithOverflow);

for (var i = 0; i < 1e4; ++i) {
    var bigValue = 1000000000000;
    var roundedValue = mathRoundWithOverflow(bigValue);
    if (roundedValue !== bigValue)
        throw "mathRoundWithOverflow(" + bigValue + ") = " + roundedValue;
}

function mathRoundConsumedAsDouble(value)
{
    return Math.round(value) * 0.5;
}
noInline(mathRoundConsumedAsDouble);

for (var i = 0; i < 1e4; ++i) {
    var doubleValue = i + 0.1;
    var roundedValue = mathRoundConsumedAsDouble(doubleValue);
    if (roundedValue !== (i * 0.5))
        throw "mathRoundConsumedAsDouble(" + doubleValue + ") = " + roundedValue;

    var doubleValue = i + 0.6;
    var roundedValue = mathRoundConsumedAsDouble(doubleValue);
    if (roundedValue !== ((i + 1) * 0.5))
        throw "mathRoundConsumedAsDouble(" + doubleValue + ") = " + roundedValue;

}

function mathRoundDoesNotCareAboutMinusZero(value)
{
    return Math.round(value)|0;
}
noInline(mathRoundDoesNotCareAboutMinusZero);

for (var i = 0; i < 1e4; ++i) {
    var doubleMid = i + 0.5;
    var roundedValue = mathRoundDoesNotCareAboutMinusZero(doubleMid);
    if (roundedValue !== i + 1)
        throw "mathRoundDoesNotCareAboutMinusZero(" + doubleMid + ") = " + roundedValue;
}


// *** Function arguments. ***
function mathRoundNoArguments()
{
    return Math.round();
}
noInline(mathRoundNoArguments);

function mathRoundTooManyArguments(a, b, c)
{
    return Math.round(a, b, c);
}
noInline(mathRoundTooManyArguments);

for (var i = 0; i < 1e4; ++i) {
    var value = mathRoundNoArguments();
    if (value === value)
        throw "mathRoundNoArguments() = " + value;

    var value = mathRoundTooManyArguments(2.1, 3, 5);
    if (value !== 2)
        throw "mathRoundTooManyArguments() = " + value;
}


// *** Constant as arguments. ***
function testMathRoundOnConstants()
{
    var value = Math.round(0);
    if (value !== 0)
        throw "Math.round(0) = " + value;
    var value = Math.round(-0);
    if (!(value === 0 && (1/value) === -Infinity))
        throw "Math.round(-0) = " + value;
    var value = Math.round(1);
    if (value !== 1)
        throw "Math.round(1) = " + value;
    var value = Math.round(-1);
    if (value !== -1)
        throw "Math.round(-1) = " + value;
    var value = Math.round(42);
    if (value !== 42)
        throw "Math.round(42) = " + value;
    var value = Math.round(-42.2);
    if (value !== -42)
        throw "Math.round(-42.2) = " + value;
    var value = Math.round(NaN);
    if (value === value)
        throw "Math.round(NaN) = " + value;
    var value = Math.round(Number.POSITIVE_INFINITY);
    if (value !== Infinity)
        throw "Math.round(Number.POSITIVE_INFINITY) = " + value;
    var value = Math.round(Number.NEGATIVE_INFINITY);
    if (value !== -Infinity)
        throw "Math.round(Number.NEGATIVE_INFINITY) = " + value;
    var value = Math.round(Math.E);
    if (value !== 3)
        throw "Math.round(Math.E) = " + value;
}
noInline(testMathRoundOnConstants);

for (var i = 0; i < 1e4; ++i) {
    testMathRoundOnConstants();
}


// *** Struct transition. ***
function mathRoundStructTransition(value)
{
    return Math.round(value);
}
noInline(mathRoundStructTransition);

for (var i = 0; i < 1e4; ++i) {
    var value = mathRoundStructTransition(42.5);
    if (value !== 43)
        throw "mathRoundStructTransition(42.5) = " + value;
}

Math.round = function() { return arguments[0] + 5; }

var value = mathRoundStructTransition(42);
if (value !== 47)
    throw "mathRoundStructTransition(42) after transition = " + value;
