function mathSignOnInteger(value)
{
    return Math.sign(value);
}
noInline(mathSignOnInteger);

// *** Test simple cases on integers. ***
function testMathSignOnIntegers()
{
    // Bounds.
    var signZero = mathSignOnInteger(0);
    if (signZero !== 0)
        throw "mathSignOnInteger(0) = " + signZero;

    var signIntMin = mathSignOnInteger(-2147483648);
    if (signIntMin !== -1)
        throw "mathSignOnInteger(-2147483648) = " + signIntMin;

    var signIntMax = mathSignOnInteger(2147483647);
    if (signIntMax !== 1)
        throw "mathSignOnInteger(2147483647) = " + signIntMax;

    // Simple values.
    var signMinusOne = mathSignOnInteger(-1);
    if (signMinusOne !== -1)
        throw "mathSignOnInteger(-1) = " + signMinusOne;

    var signOne = mathSignOnInteger(1);
    if (signOne !== 1)
        throw "mathSignOnInteger(1) = " + signOne;

    var signFortyTwo = mathSignOnInteger(42);
    if (signFortyTwo !== 1)
        throw "mathSignOnInteger(42) = " + signFortyTwo;

    var signMinusFortyTwo = mathSignOnInteger(-42);
    if (signMinusFortyTwo !== -1)
        throw "mathSignOnInteger(-42) = " + signMinusFortyTwo;
}
noInline(testMathSignOnIntegers);

for (var i = 0; i < 1e4; ++i) {
    testMathSignOnIntegers();
}

// Make sure we don't do anything stupid when the type is unexpected.
function verifySignWithObject()
{
    var signObject = mathSignOnInteger({ valueOf: function() { return 7; } });
    if (signObject !== 1)
        throw "mathSignOnInteger({ valueOf: function() { return 7; } }) = " + signObject;

    var signNegObject = mathSignOnInteger({ valueOf: function() { return -7; } });
    if (signNegObject !== -1)
        throw "mathSignOnInteger({ valueOf: function() { return -7; } }) = " + signNegObject;

    var signString = mathSignOnInteger("WebKit");
    if (!isNaN(signString))
        throw "mathSignOnInteger(\"WebKit\") = " + signString;
}
noInline(verifySignWithObject);

for (var i = 0; i < 1e4; ++i) {
    verifySignWithObject();
}

// *** Test simple cases on doubles. ***
function mathSignOnDouble(value)
{
    return Math.sign(value);
}
noInline(mathSignOnDouble);

function testMathSignOnDoubles()
{
    var signNaN = mathSignOnDouble(NaN);
    if (!isNaN(signNaN))
        throw "mathSignOnDouble(NaN) = " + signNaN;

    var signPositiveZero = mathSignOnDouble(0.0);
    if (!Object.is(signPositiveZero, 0))
        throw "mathSignOnDouble(0.0) = " + signPositiveZero;

    var signNegativeZero = mathSignOnDouble(-0.0);
    if (!Object.is(signNegativeZero, -0))
        throw "mathSignOnDouble(-0.0) = " + signNegativeZero;

    var signPositiveInfinity = mathSignOnDouble(Infinity);
    if (signPositiveInfinity !== 1)
        throw "mathSignOnDouble(Infinity) = " + signPositiveInfinity;

    var signNegativeInfinity = mathSignOnDouble(-Infinity);
    if (signNegativeInfinity !== -1)
        throw "mathSignOnDouble(-Infinity) = " + signNegativeInfinity;

    var signMinValue = mathSignOnDouble(Number.MIN_VALUE);
    if (signMinValue !== 1)
        throw "mathSignOnDouble(Number.MIN_VALUE) = " + signMinValue;

    var signNegMinValue = mathSignOnDouble(-Number.MIN_VALUE);
    if (signNegMinValue !== -1)
        throw "mathSignOnDouble(-Number.MIN_VALUE) = " + signNegMinValue;

    var signMaxValue = mathSignOnDouble(Number.MAX_VALUE);
    if (signMaxValue !== 1)
        throw "mathSignOnDouble(Number.MAX_VALUE) = " + signMaxValue;

    var signEpsilon = mathSignOnDouble(Number.EPSILON);
    if (signEpsilon !== 1)
        throw "mathSignOnDouble(Number.EPSILON) = " + signEpsilon;

    var signPositiveFractional = mathSignOnDouble(0.5);
    if (signPositiveFractional !== 1)
        throw "mathSignOnDouble(0.5) = " + signPositiveFractional;

    var signNegativeFractional = mathSignOnDouble(-0.5);
    if (signNegativeFractional !== -1)
        throw "mathSignOnDouble(-0.5) = " + signNegativeFractional;
}
noInline(testMathSignOnDoubles);

for (var i = 0; i < 1e4; ++i) {
    testMathSignOnDoubles();
}

// *** Unusual arguments. ***
function mathSignNoArguments()
{
    return Math.sign();
}
noInline(mathSignNoArguments);

function mathSignTooManyArguments(a, b, c)
{
    return Math.sign(a, b, c);
}
noInline(mathSignTooManyArguments);

for (var i = 0; i < 1e4; ++i) {
    var signNoArguments = mathSignNoArguments();
    if (!isNaN(signNoArguments))
        throw "Math.sign() = " + signNoArguments;

    var signTooManyArguments = mathSignTooManyArguments(2, 3, 5);
    if (signTooManyArguments !== 1)
        throw "mathSignTooManyArguments(2, 3, 5) = " + signTooManyArguments;
}

// *** Constant as arguments. ***
function testMathSignOnConstants()
{
    var signZero = Math.sign(0);
    if (!Object.is(signZero, 0))
        throw "Math.sign(0) = " + signZero;

    var signNegativeZero = Math.sign(-0.0);
    if (!Object.is(signNegativeZero, -0))
        throw "Math.sign(-0.0) = " + signNegativeZero;

    var signNaN = Math.sign(NaN);
    if (!isNaN(signNaN))
        throw "Math.sign(NaN) = " + signNaN;

    var signPositive = Math.sign(42);
    if (signPositive !== 1)
        throw "Math.sign(42) = " + signPositive;

    var signNegative = Math.sign(-42);
    if (signNegative !== -1)
        throw "Math.sign(-42) = " + signNegative;

    var signPositiveDouble = Math.sign(42.5);
    if (signPositiveDouble !== 1)
        throw "Math.sign(42.5) = " + signPositiveDouble;

    var signNegativeDouble = Math.sign(-42.5);
    if (signNegativeDouble !== -1)
        throw "Math.sign(-42.5) = " + signNegativeDouble;

    var signIntMin = Math.sign(-2147483648);
    if (signIntMin !== -1)
        throw "Math.sign(-2147483648) = " + signIntMin;

    var signInfinity = Math.sign(Infinity);
    if (signInfinity !== 1)
        throw "Math.sign(Infinity) = " + signInfinity;

    var signNegativeInfinity = Math.sign(-Infinity);
    if (signNegativeInfinity !== -1)
        throw "Math.sign(-Infinity) = " + signNegativeInfinity;
}
noInline(testMathSignOnConstants);

for (var i = 0; i < 1e4; ++i) {
    testMathSignOnConstants();
}

// *** Type Coercion ***
function mathSignStructTransition(value)
{
    return Math.sign(value);
}
noInline(mathSignStructTransition);

function testMathSignOnStructTransition()
{
    var signObj1 = mathSignStructTransition({ valueOf: function() { return -1; } });
    if (signObj1 !== -1)
        throw "mathSignStructTransition({ valueOf: function() { return -1; } }) = " + signObj1;

    var signObj2 = mathSignStructTransition({ a: "a", valueOf: function() { return 5; } });
    if (signObj2 !== 1)
        throw "mathSignStructTransition({ a: 'a', valueOf: function() { return 5; } }) = " + signObj2;

    var signString = mathSignStructTransition("-3");
    if (signString !== -1)
        throw "mathSignStructTransition(\"-3\") = " + signString;

    var signTrue = mathSignStructTransition(true);
    if (signTrue !== 1)
        throw "mathSignStructTransition(true) = " + signTrue;

    var signFalse = mathSignStructTransition(false);
    if (!Object.is(signFalse, 0))
        throw "mathSignStructTransition(false) = " + signFalse;

    var signNull = mathSignStructTransition(null);
    if (!Object.is(signNull, 0))
        throw "mathSignStructTransition(null) = " + signNull;

    var signUndefined = mathSignStructTransition(undefined);
    if (!isNaN(signUndefined))
        throw "mathSignStructTransition(undefined) = " + signUndefined;
}
noInline(testMathSignOnStructTransition);

for (var i = 0; i < 1e4; ++i) {
    testMathSignOnStructTransition();
}
