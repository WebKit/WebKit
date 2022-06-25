function mathClz32OnInteger(value)
{
    return Math.clz32(value);
}
noInline(mathClz32OnInteger);

// *** Test simple cases on integers. ***
function testMathClz32OnIntegers()
{
    // Bounds.
    var clzZero = mathClz32OnInteger(0);
    if (clzZero != 32)
        throw "mathClz32OnInteger(0) = " + clzZero;

    var clzIntMin = mathClz32OnInteger(-2147483648);
    if (clzIntMin != 0)
        throw "mathClz32OnInteger(-2147483648) = " + clzIntMin;

    var clzIntMax = mathClz32OnInteger(2147483647);
    if (clzIntMax != 1)
        throw "mathClz32OnInteger(2147483647) = " + clzIntMax;

    // Simple values.
    var clzMinusOne = mathClz32OnInteger(-1);
    if (clzMinusOne != 0)
        throw "mathClz32OnInteger(-1) = " + clzMinusOne;

    var clzUltimateAnswer = mathClz32OnInteger(42);
    if (clzUltimateAnswer != 26)
        throw "mathClz32OnInteger(42) = " + clzUltimateAnswer;

    var clzMinusUltimateAnswer = mathClz32OnInteger(-42);
    if (clzMinusUltimateAnswer != 0)
        throw "mathClz32OnInteger(-42) = " + clzMinusUltimateAnswer;
}
noInline(testMathClz32OnIntegers);

for (var i = 0; i < 1e4; ++i) {
    testMathClz32OnIntegers();
}

// Make sure we don't do anything stupid when the type is unexpected.
function verifyMathClz32OnIntegerWithOtherTypes()
{
    var clzPi = mathClz32OnInteger(Math.PI);
    if (clzPi != 30)
        throw "mathClz32OnInteger(Math.PI) = " + clzPi;

    var clzString = mathClz32OnInteger("42");
    if (clzString != 26)
        throw "mathClz32OnInteger(\"42\") = " + clzString;

    var clzString = mathClz32OnInteger("WebKit");
    if (clzString != 32)
        throw "mathClz32OnInteger(\"WebKit\") = " + clzString;

    var clzMinusZero = mathClz32OnInteger(-0);
    if (clzMinusZero != 32)
        throw "mathClz32OnInteger(-0) = " + clzMinusZero;
}
noInline(verifyMathClz32OnIntegerWithOtherTypes);

for (var i = 0; i < 1e4; ++i) {
    verifyMathClz32OnIntegerWithOtherTypes();
}


// *** Test simple cases on doubles. ***
function mathClz32OnDouble(value)
{
    return Math.clz32(value);
}
noInline(mathClz32OnDouble);

// Test simple cases on doubles.
function testMathClz32OnDoubles()
{
    var value = mathClz32OnDouble(Math.PI);
    if (value != 30)
        throw "mathClz32OnDouble(Math.PI) = " + value;

    var value = mathClz32OnDouble(Math.E);
    if (value != 30)
        throw "mathClz32OnDouble(Math.E) = " + value;

    var value = mathClz32OnDouble(Math.LN2);
    if (value != 32)
        throw "mathClz32OnDouble(Math.LN2) = " + value;

    var value = mathClz32OnDouble(-0);
    if (value != 32)
        throw "mathClz32OnDouble(-0) = " + value;

    var value = mathClz32OnDouble(NaN);
    if (value != 32)
        throw "mathClz32OnDouble(NaN) = " + value;

    var value = mathClz32OnDouble(Number.POSITIVE_INFINITY);
    if (value != 32)
        throw "mathClz32OnDouble(Number.POSITIVE_INFINITY) = " + value;

    var value = mathClz32OnDouble(Number.NEGATIVE_INFINITY);
    if (value != 32)
        throw "mathClz32OnDouble(Number.NEGATIVE_INFINITY) = " + value;
}
noInline(testMathClz32OnDoubles);

for (var i = 0; i < 1e4; ++i) {
    testMathClz32OnDoubles();
}

// Make sure we don't do anything stupid when the type is unexpected.
function verifyMathClz32OnDoublesWithOtherTypes()
{
    var clzOne = mathClz32OnDouble(1);
    if (clzOne != 31)
        throw "mathClz32OnDouble(1) = " + clzOne;

    var clzString = mathClz32OnDouble("42");
    if (clzString != 26)
        throw "mathClz32OnDouble(\"42\") = " + clzString;

    var clzString = mathClz32OnDouble("WebKit");
    if (clzString != 32)
        throw "mathClz32OnDouble(\"WebKit\") = " + clzString;

    var clzMinusZero = mathClz32OnDouble({});
    if (clzMinusZero != 32)
        throw "mathClz32OnDouble({}) = " + clzMinusZero;
}
noInline(verifyMathClz32OnDoublesWithOtherTypes);

for (var i = 0; i < 1e4; ++i) {
    verifyMathClz32OnDoublesWithOtherTypes();
}


// *** Unusual arguments. ***
function mathClz32NoArguments()
{
    return Math.clz32();
}
noInline(mathClz32NoArguments);

function mathClz32TooManyArguments(a, b, c)
{
    return Math.clz32(a, b, c);
}
noInline(mathClz32TooManyArguments);


for (var i = 0; i < 1e4; ++i) {
    var value = mathClz32NoArguments();
    if (value !== 32)
        throw "mathClz32NoArguments() = " + value;

    var value = mathClz32TooManyArguments(2, 3, 5);
    if (value !== 30)
        throw "mathClz32TooManyArguments() = " + value;

}


// *** Constant as arguments. ***
function testMathClz32OnConstants()
{
    var value = Math.clz32(0);
    if (value !== 32)
        throw "Math.clz32(0) = " + value;
    var value = Math.clz32(-0);
    if (value !== 32)
        throw "Math.clz32(-0) = " + value;
    var value = Math.clz32(1);
    if (value !== 31)
        throw "Math.clz32(1) = " + value;
    var value = Math.clz32(-1);
    if (value !== 0)
        throw "Math.clz32(-1) = " + value;
    var value = Math.clz32(42);
    if (value !== 26)
        throw "Math.clz32(42) = " + value;
    var value = Math.clz32(-42);
    if (value !== 0)
        throw "Math.clz32(-42) = " + value;
    var value = Math.clz32(NaN);
    if (value !== 32)
        throw "Math.clz32(NaN) = " + value;
    var value = Math.clz32(Number.POSITIVE_INFINITY);
    if (value !== 32)
        throw "Math.clz32(Number.POSITIVE_INFINITY) = " + value;
    var value = Math.clz32(Number.NEGATIVE_INFINITY);
    if (value !== 32)
        throw "Math.clz32(Number.NEGATIVE_INFINITY) = " + value;
    var value = Math.clz32(Math.E);
    if (value !== 30)
        throw "Math.clz32(Math.E) = " + value;
}
noInline(testMathClz32OnConstants);

for (var i = 0; i < 1e4; ++i) {
    testMathClz32OnConstants();
}


// *** Struct transition. ***
function mathClz32StructTransition(value)
{
    return Math.clz32(value);
}
noInline(mathClz32StructTransition);

for (var i = 0; i < 1e4; ++i) {
    var value = mathClz32StructTransition(42);
    if (value !== 26)
        throw "mathClz32StructTransition(42) = " + value;
}

Math.clz32 = function() { return arguments[0] + 5; }

var value = mathClz32StructTransition(42);
if (value !== 47)
    throw "mathClz32StructTransition(42) after transition = " + value;
