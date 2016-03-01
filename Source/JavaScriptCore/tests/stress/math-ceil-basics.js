
function mathCeilOnIntegers(value)
{
    return Math.ceil(value);
}
noInline(mathCeilOnIntegers);

function mathCeilOnDoubles(value)
{
    return Math.ceil(value);
}
noInline(mathCeilOnDoubles);

function mathCeilOnBooleans(value)
{
    return Math.ceil(value);
}
noInline(mathCeilOnBooleans);

// The trivial cases first.
for (var i = 1; i < 1e4; ++i) {
    var ceiledValue = mathCeilOnIntegers(i);
    if (ceiledValue !== i)
        throw new Error("mathCeilOnIntegers(" + i + ") = " + ceiledValue);

    var ceiledValue = mathCeilOnIntegers(-i);
    if (ceiledValue !== -i)
        throw new Error("mathCeilOnIntegers(" + -i + ") = " + ceiledValue);

    var doubleLow = i + 0.4;
    var ceiledValue = mathCeilOnDoubles(doubleLow);
    if (ceiledValue !== i + 1.0)
        throw new Error("mathCeilOnDoubles(" + doubleLow + ") = " + ceiledValue);

    var doubleHigh = i + 0.6;
    var ceiledValue = mathCeilOnDoubles(doubleHigh);
    if (ceiledValue !== i + 1)
        throw new Error("mathCeilOnDoubles(" + doubleHigh + ") = " + ceiledValue);

    var doubleMid = i + 0.5;
    var ceiledValue = mathCeilOnDoubles(doubleMid);
    if (ceiledValue !== i + 1)
        throw new Error("mathCeilOnDoubles(" + doubleMid + ") = " + ceiledValue);

    var ceiledValue = mathCeilOnDoubles(-0.6);
    if (ceiledValue !== -0.0)
        throw new Error("mathCeilOnDoubles(-0.6) = " + ceiledValue);
}

// Some more interesting cases, some of them well OSR exit when the return value is zero.
for (var i = 0; i < 1e4; ++i) {
    var ceiledValue = mathCeilOnIntegers(i);
    if (ceiledValue !== i)
        throw new Error("mathCeilOnIntegers(" + i + ") = " + ceiledValue);

    var ceiledValue = mathCeilOnIntegers(-i);
    if (ceiledValue !== -i)
        throw new Error("mathCeilOnIntegers(-" + i + ") = " + ceiledValue);

    var ceiledValue = mathCeilOnDoubles(-0.4);
    if (ceiledValue !== 0)
        throw new Error("mathCeilOnDoubles(-0.4) = " + ceiledValue);

    var ceiledValue = mathCeilOnDoubles(-0.5);
    if (ceiledValue !== 0)
        throw new Error("mathCeilOnDoubles(-0.5) = " + ceiledValue);

    var ceiledValue = mathCeilOnDoubles(-0);
    if (!(ceiledValue === 0 && (1/ceiledValue) === -Infinity))
        throw new Error("mathCeilOnDoubles(-0) = " + ceiledValue);

    var ceiledValue = mathCeilOnDoubles(NaN);
    if (ceiledValue === ceiledValue)
        throw new Error("mathCeilOnDoubles(NaN) = " + ceiledValue);

    var ceiledValue = mathCeilOnDoubles(Number.POSITIVE_INFINITY);
    if (ceiledValue !== Number.POSITIVE_INFINITY)
        throw new Error("mathCeilOnDoubles(Number.POSITIVE_INFINITY) = " + ceiledValue);

    var ceiledValue = mathCeilOnDoubles(Number.NEGATIVE_INFINITY);
    if (ceiledValue !== Number.NEGATIVE_INFINITY)
        throw new Error("mathCeilOnDoubles(Number.NEGATIVE_INFINITY) = " + ceiledValue);

    var boolean = !!(i % 2);
    var ceiledBoolean = mathCeilOnBooleans(boolean);
    if (ceiledBoolean != boolean)
        throw new Error("mathCeilOnDoubles(" + boolean + ") = " + ceiledBoolean);
}

function uselessMathCeil(value)
{
    return Math.ceil(value|0);
}
noInline(uselessMathCeil);

for (var i = 0; i < 1e4; ++i) {
    var ceiledValue = uselessMathCeil(i);
    if (ceiledValue !== i)
        throw new Error("uselessMathCeil(" + i + ") = " + ceiledValue);

    var doubleLow = i + 0.4;
    var ceiledValue = uselessMathCeil(doubleLow);
    if (ceiledValue !== i)
        throw new Error("uselessMathCeil(" + doubleLow + ") = " + ceiledValue);

    var doubleHigh = i + 0.6;
    var ceiledValue = uselessMathCeil(doubleHigh);
    if (ceiledValue !== i)
        throw new Error("uselessMathCeil(" + doubleHigh + ") = " + ceiledValue);

    var doubleMid = i + 0.5;
    var ceiledValue = uselessMathCeil(doubleMid);
    if (ceiledValue !== i)
        throw new Error("uselessMathCeil(" + doubleMid + ") = " + ceiledValue);

    var ceiledValue = uselessMathCeil(-0.4);
    if (ceiledValue !== 0)
        throw new Error("uselessMathCeil(-0.4) = " + ceiledValue);

    var ceiledValue = uselessMathCeil(-0.5);
    if (ceiledValue !== 0)
        throw new Error("uselessMathCeil(-0.5) = " + ceiledValue);

    var ceiledValue = uselessMathCeil(-0.6);
    if (ceiledValue !== 0)
        throw new Error("uselessMathCeil(-0.6) = " + ceiledValue);
}

function mathCeilWithOverflow(value)
{
    return Math.ceil(value);
}
noInline(mathCeilWithOverflow);

for (var i = 0; i < 1e4; ++i) {
    var bigValue = 1000000000000;
    var ceiledValue = mathCeilWithOverflow(bigValue);
    if (ceiledValue !== bigValue)
        throw new Error("mathCeilWithOverflow(" + bigValue + ") = " + ceiledValue);
}

function mathCeilConsumedAsDouble(value)
{
    return Math.ceil(value) * 0.5;
}
noInline(mathCeilConsumedAsDouble);

for (var i = 0; i < 1e4; ++i) {
    var doubleValue = i + 0.1;
    var ceiledValue = mathCeilConsumedAsDouble(doubleValue);
    if (ceiledValue !== ((i + 1) * 0.5))
        throw new Error("mathCeilConsumedAsDouble(" + doubleValue + ") = " + ceiledValue);

    var doubleValue = i + 0.6;
    var ceiledValue = mathCeilConsumedAsDouble(doubleValue);
    if (ceiledValue !== ((i + 1) * 0.5))
        throw new Error("mathCeilConsumedAsDouble(" + doubleValue + ") = " + ceiledValue);

}

function mathCeilDoesNotCareAboutMinusZero(value)
{
    return Math.ceil(value)|0;
}
noInline(mathCeilDoesNotCareAboutMinusZero);

for (var i = 0; i < 1e4; ++i) {
    var doubleMid = i + 0.5;
    var ceiledValue = mathCeilDoesNotCareAboutMinusZero(doubleMid);
    if (ceiledValue !== i + 1)
        throw new Error("mathCeilDoesNotCareAboutMinusZero(" + doubleMid + ") = " + ceiledValue);
}


// *** Function arguments. ***
function mathCeilNoArguments()
{
    return Math.ceil();
}
noInline(mathCeilNoArguments);

function mathCeilTooManyArguments(a, b, c)
{
    return Math.ceil(a, b, c);
}
noInline(mathCeilTooManyArguments);

for (var i = 0; i < 1e4; ++i) {
    var value = mathCeilNoArguments();
    if (value === value)
        throw new Error("mathCeilNoArguments() = " + value);

    var value = mathCeilTooManyArguments(2.1, 3, 5);
    if (value !== 3)
        throw new Error("mathCeilTooManyArguments() = " + value);
}


// *** Constant as arguments. ***
function testMathCeilOnConstants()
{
    var value = Math.ceil(0);
    if (value !== 0)
        throw new Error("Math.ceil(0) = " + value);
    var value = Math.ceil(-0);
    if (!(value === 0 && (1/value) === -Infinity))
        throw new Error("Math.ceil(-0) = " + value);
    var value = Math.ceil(1);
    if (value !== 1)
        throw new Error("Math.ceil(1) = " + value);
    var value = Math.ceil(-1);
    if (value !== -1)
        throw new Error("Math.ceil(-1) = " + value);
    var value = Math.ceil(42);
    if (value !== 42)
        throw new Error("Math.ceil(42) = " + value);
    var value = Math.ceil(-42.2);
    if (value !== -42)
        throw new Error("Math.ceil(-42.2) = " + value);
    var value = Math.ceil(NaN);
    if (value === value)
        throw new Error("Math.ceil(NaN) = " + value);
    var value = Math.ceil(Number.POSITIVE_INFINITI);
    if (value === value)
        throw new Error("Math.ceil(Number.POSITIVE_INFINITI) = " + value);
    var value = Math.ceil(Number.NEGATIVE_INFINITI);
    if (value === value)
        throw new Error("Math.ceil(Number.NEGATIVE_INFINITI) = " + value);
    var value = Math.ceil(Math.E);
    if (value !== 3)
        throw new Error("Math.ceil(Math.E) = " + value);
}
noInline(testMathCeilOnConstants);

for (var i = 0; i < 1e4; ++i) {
    testMathCeilOnConstants();
}


// *** Struct transition. ***
function mathCeilStructTransition(value)
{
    return Math.ceil(value);
}
noInline(mathCeilStructTransition);

for (var i = 0; i < 1e4; ++i) {
    var value = mathCeilStructTransition(42.5);
    if (value !== 43)
        throw new Error("mathCeilStructTransition(42.5) = " + value);
}

Math.ceil = function() { return arguments[0] + 5; }

var value = mathCeilStructTransition(42);
if (value !== 47)
    throw new Error("mathCeilStructTransition(42) after transition = " + value);
