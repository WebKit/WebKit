
function mathFloorOnIntegers(value)
{
    return Math.floor(value);
}
noInline(mathFloorOnIntegers);

function mathFloorOnDoubles(value)
{
    return Math.floor(value);
}
noInline(mathFloorOnDoubles);

function mathFloorOnBooleans(value)
{
    return Math.floor(value);
}
noInline(mathFloorOnBooleans);

// The trivial cases first.
for (var i = 1; i < 1e4; ++i) {
    var flooredValue = mathFloorOnIntegers(i);
    if (flooredValue !== i)
        throw new Error("mathFloorOnIntegers(" + i + ") = " + flooredValue);

    var flooredValue = mathFloorOnIntegers(-i);
    if (flooredValue !== -i)
        throw new Error("mathFloorOnIntegers(" + -i + ") = " + flooredValue);

    var doubleLow = i + 0.4;
    var flooredValue = mathFloorOnDoubles(doubleLow);
    if (flooredValue !== i)
        throw new Error("mathFloorOnDoubles(" + doubleLow + ") = " + flooredValue);

    var doubleHigh = i + 0.6;
    var flooredValue = mathFloorOnDoubles(doubleHigh);
    if (flooredValue !== i)
        throw new Error("mathFloorOnDoubles(" + doubleHigh + ") = " + flooredValue);

    var doubleMid = i + 0.5;
    var flooredValue = mathFloorOnDoubles(doubleMid);
    if (flooredValue !== i)
        throw new Error("mathFloorOnDoubles(" + doubleMid + ") = " + flooredValue);

    var flooredValue = mathFloorOnDoubles(-0.6);
    if (flooredValue !== -1.0)
        throw new Error("mathFloorOnDoubles(-0.6) = " + flooredValue);
}

// Some more interesting cases, some of them well OSR exit when the return value is zero.
for (var i = 0; i < 1e4; ++i) {
    var flooredValue = mathFloorOnIntegers(i);
    if (flooredValue !== i)
        throw new Error("mathFloorOnIntegers(" + i + ") = " + flooredValue);

    var flooredValue = mathFloorOnIntegers(-i);
    if (flooredValue !== -i)
        throw new Error("mathFloorOnIntegers(-" + i + ") = " + flooredValue);

    var flooredValue = mathFloorOnDoubles(-0.4);
    if (flooredValue !== -1.00)
        throw new Error("mathFloorOnDoubles(-0.4) = " + flooredValue);

    var flooredValue = mathFloorOnDoubles(-0.5);
    if (flooredValue !== -1.0)
        throw new Error("mathFloorOnDoubles(-0.5) = " + flooredValue);

    var flooredValue = mathFloorOnDoubles(-0);
    if (!(flooredValue === 0 && (1/flooredValue) === -Infinity))
        throw new Error("mathFloorOnDoubles(-0) = " + flooredValue);

    var flooredValue = mathFloorOnDoubles(NaN);
    if (flooredValue === flooredValue)
        throw new Error("mathFloorOnDoubles(NaN) = " + flooredValue);

    var flooredValue = mathFloorOnDoubles(Number.POSITIVE_INFINITY);
    if (flooredValue !== Number.POSITIVE_INFINITY)
        throw new Error("mathFloorOnDoubles(Number.POSITIVE_INFINITY) = " + flooredValue);

    var flooredValue = mathFloorOnDoubles(Number.NEGATIVE_INFINITY);
    if (flooredValue !== Number.NEGATIVE_INFINITY)
        throw new Error("mathFloorOnDoubles(Number.NEGATIVE_INFINITY) = " + flooredValue);

    var boolean = !!(i % 2);
    var flooredBoolean = mathFloorOnBooleans(boolean);
    if (flooredBoolean != boolean)
        throw new Error("mathFloorOnDoubles(" + boolean + ") = " + flooredBoolean);
}

function uselessMathFloor(value)
{
    return Math.floor(value|0);
}
noInline(uselessMathFloor);

for (var i = 0; i < 1e4; ++i) {
    var flooredValue = uselessMathFloor(i);
    if (flooredValue !== i)
        throw new Error("uselessMathFloor(" + i + ") = " + flooredValue);

    var doubleLow = i + 0.4;
    var flooredValue = uselessMathFloor(doubleLow);
    if (flooredValue !== i)
        throw new Error("uselessMathFloor(" + doubleLow + ") = " + flooredValue);

    var doubleHigh = i + 0.6;
    var flooredValue = uselessMathFloor(doubleHigh);
    if (flooredValue !== i)
        throw new Error("uselessMathFloor(" + doubleHigh + ") = " + flooredValue);

    var doubleMid = i + 0.5;
    var flooredValue = uselessMathFloor(doubleMid);
    if (flooredValue !== i)
        throw new Error("uselessMathFloor(" + doubleMid + ") = " + flooredValue);

    var flooredValue = uselessMathFloor(-0.4);
    if (flooredValue !== 0)
        throw new Error("uselessMathFloor(-0.4) = " + flooredValue);

    var flooredValue = uselessMathFloor(-0.5);
    if (flooredValue !== 0)
        throw new Error("uselessMathFloor(-0.5) = " + flooredValue);

    var flooredValue = uselessMathFloor(-0.6);
    if (flooredValue !== 0)
        throw new Error("uselessMathFloor(-0.6) = " + flooredValue);
}

function mathFloorWithOverflow(value)
{
    return Math.floor(value);
}
noInline(mathFloorWithOverflow);

for (var i = 0; i < 1e4; ++i) {
    var bigValue = 1000000000000;
    var flooredValue = mathFloorWithOverflow(bigValue);
    if (flooredValue !== bigValue)
        throw new Error("mathFloorWithOverflow(" + bigValue + ") = " + flooredValue);
}

function mathFloorConsumedAsDouble(value)
{
    return Math.floor(value) * 0.5;
}
noInline(mathFloorConsumedAsDouble);

for (var i = 0; i < 1e4; ++i) {
    var doubleValue = i + 0.1;
    var flooredValue = mathFloorConsumedAsDouble(doubleValue);
    if (flooredValue !== (i * 0.5))
        throw new Error("mathFloorConsumedAsDouble(" + doubleValue + ") = " + flooredValue);

    var doubleValue = i + 0.6;
    var flooredValue = mathFloorConsumedAsDouble(doubleValue);
    if (flooredValue !== (i * 0.5))
        throw new Error("mathFloorConsumedAsDouble(" + doubleValue + ") = " + flooredValue);

}

function mathFloorDoesNotCareAboutMinusZero(value)
{
    return Math.floor(value)|0;
}
noInline(mathFloorDoesNotCareAboutMinusZero);

for (var i = 0; i < 1e4; ++i) {
    var doubleMid = i + 0.5;
    var flooredValue = mathFloorDoesNotCareAboutMinusZero(doubleMid);
    if (flooredValue !== i)
        throw new Error("mathFloorDoesNotCareAboutMinusZero(" + doubleMid + ") = " + flooredValue);
}


// *** Function arguments. ***
function mathFloorNoArguments()
{
    return Math.floor();
}
noInline(mathFloorNoArguments);

function mathFloorTooManyArguments(a, b, c)
{
    return Math.floor(a, b, c);
}
noInline(mathFloorTooManyArguments);

for (var i = 0; i < 1e4; ++i) {
    var value = mathFloorNoArguments();
    if (value === value)
        throw new Error("mathFloorNoArguments() = " + value);

    var value = mathFloorTooManyArguments(2.1, 3, 5);
    if (value !== 2)
        throw new Error("mathFloorTooManyArguments() = " + value);
}


// *** Constant as arguments. ***
function testMathFloorOnConstants()
{
    var value = Math.floor(0);
    if (value !== 0)
        throw new Error("Math.floor(0) = " + value);
    var value = Math.floor(-0);
    if (!(value === 0 && (1/value) === -Infinity))
        throw new Error("Math.floor(-0) = " + value);
    var value = Math.floor(1);
    if (value !== 1)
        throw new Error("Math.floor(1) = " + value);
    var value = Math.floor(-1);
    if (value !== -1)
        throw new Error("Math.floor(-1) = " + value);
    var value = Math.floor(42);
    if (value !== 42)
        throw new Error("Math.floor(42) = " + value);
    var value = Math.floor(-42.2);
    if (value !== -43)
        throw new Error("Math.floor(-42.2) = " + value);
    var value = Math.floor(NaN);
    if (value === value)
        throw new Error("Math.floor(NaN) = " + value);
    var value = Math.floor(Number.POSITIVE_INFINITI);
    if (value === value)
        throw new Error("Math.floor(Number.POSITIVE_INFINITI) = " + value);
    var value = Math.floor(Number.NEGATIVE_INFINITI);
    if (value === value)
        throw new Error("Math.floor(Number.NEGATIVE_INFINITI) = " + value);
    var value = Math.floor(Math.E);
    if (value !== 2)
        throw new Error("Math.floor(Math.E) = " + value);
}
noInline(testMathFloorOnConstants);

for (var i = 0; i < 1e4; ++i) {
    testMathFloorOnConstants();
}


// *** Struct transition. ***
function mathFloorStructTransition(value)
{
    return Math.floor(value);
}
noInline(mathFloorStructTransition);

for (var i = 0; i < 1e4; ++i) {
    var value = mathFloorStructTransition(42.5);
    if (value !== 42)
        throw new Error("mathFloorStructTransition(42.5) = " + value);
}

Math.floor = function() { return arguments[0] + 5; }

var value = mathFloorStructTransition(42);
if (value !== 47)
    throw new Error("mathFloorStructTransition(42) after transition = " + value);
