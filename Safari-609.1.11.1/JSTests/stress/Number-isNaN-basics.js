function numberIsNaNOnInteger(value)
{
    return Number.isNaN(value);
}
noInline(numberIsNaNOnInteger);

// *** Test simple cases on integers. ***
function testNumberIsNaNOnIntegers()
{
    // Bounds.
    var value = numberIsNaNOnInteger(0);
    if (value)
        throw "numberIsNaNOnInteger(0) = " + value;

    var value = numberIsNaNOnInteger(-2147483648);
    if (value)
        throw "numberIsNaNOnInteger(-2147483648) = " + value;

    var value = numberIsNaNOnInteger(2147483647);
    if (value)
        throw "numberIsNaNOnInteger(2147483647) = " + value;

    // Simple values.
    var value = numberIsNaNOnInteger(-1);
    if (value)
        throw "numberIsNaNOnInteger(-1) = " + value;

    var value = numberIsNaNOnInteger(42);
    if (value)
        throw "numberIsNaNOnInteger(42) = " + value;

    var value = numberIsNaNOnInteger(-42);
    if (value)
        throw "numberIsNaNOnInteger(-42) = " + value;
}
noInline(testNumberIsNaNOnIntegers);

for (var i = 0; i < 1e4; ++i) {
    testNumberIsNaNOnIntegers();
}

// Make sure we don't do anything stupid when the type is unexpected.
function verifyNumberIsNaNOnIntegerWithOtherTypes()
{
    var value = numberIsNaNOnInteger(Math.PI);
    if (value)
        throw "numberIsNaNOnInteger(Math.PI) = " + value;

    var value = numberIsNaNOnInteger("42");
    if (value)
        throw "numberIsNaNOnInteger(\"42\") = " + value;

    var value = numberIsNaNOnInteger("WebKit");
    if (value)
        throw "numberIsNaNOnInteger(\"WebKit\") = " + value;

    var value = numberIsNaNOnInteger(-0);
    if (value)
        throw "numberIsNaNOnInteger(-0) = " + value;
}
noInline(verifyNumberIsNaNOnIntegerWithOtherTypes);

for (var i = 0; i < 1e4; ++i) {
    verifyNumberIsNaNOnIntegerWithOtherTypes();
}


// *** Test simple cases on doubles. ***
function numberIsNaNOnDouble(value)
{
    return Number.isNaN(value);
}
noInline(numberIsNaNOnDouble);

// Test simple cases on doubles.
function testNumberIsNaNOnDoubles()
{
    var value = numberIsNaNOnDouble(Math.PI);
    if (value)
        throw "numberIsNaNOnDouble(Math.PI) = " + value;

    var value = numberIsNaNOnDouble(Math.E);
    if (value)
        throw "numberIsNaNOnDouble(Math.E) = " + value;

    var value = numberIsNaNOnDouble(Math.LN2);
    if (value)
        throw "numberIsNaNOnDouble(Math.LN2) = " + value;

    var value = numberIsNaNOnDouble(-0);
    if (value)
        throw "numberIsNaNOnDouble(-0) = " + value;

    var value = numberIsNaNOnDouble(NaN);
    if (!value)
        throw "numberIsNaNOnDouble(NaN) = " + value;

    var value = numberIsNaNOnDouble(Number.POSITIVE_INFINITY);
    if (value)
        throw "numberIsNaNOnDouble(Number.POSITIVE_INFINITY) = " + value;

    var value = numberIsNaNOnDouble(Number.NEGATIVE_INFINITY);
    if (value)
        throw "numberIsNaNOnDouble(Number.NEGATIVE_INFINITY) = " + value;
}
noInline(testNumberIsNaNOnDoubles);

for (var i = 0; i < 1e4; ++i) {
    testNumberIsNaNOnDoubles();
}

// Make sure we don't do anything stupid when the type is unexpected.
function verifyNumberIsNaNOnDoublesWithOtherTypes()
{
    var value = numberIsNaNOnDouble(1);
    if (value)
        throw "numberIsNaNOnDouble(1) = " + value;

    var value = numberIsNaNOnDouble("42");
    if (value)
        throw "numberIsNaNOnDouble(\"42\") = " + value;

    var value = numberIsNaNOnDouble("WebKit");
    if (value)
        throw "numberIsNaNOnDouble(\"WebKit\") = " + value;

    var value = numberIsNaNOnDouble({});
    if (value)
        throw "numberIsNaNOnDouble({}) = " + value;
}
noInline(verifyNumberIsNaNOnDoublesWithOtherTypes);

for (var i = 0; i < 1e4; ++i) {
    verifyNumberIsNaNOnDoublesWithOtherTypes();
}


// *** Unusual arguments. ***
function numberIsNaNNoArguments()
{
    return Number.isNaN();
}
noInline(numberIsNaNNoArguments);

function numberIsNaNTooManyArguments(a, b, c)
{
    return Number.isNaN(a, b, c);
}
noInline(numberIsNaNTooManyArguments);


for (var i = 0; i < 1e4; ++i) {
    var value = numberIsNaNNoArguments();
    if (value)
        throw "numberIsNaNNoArguments() = " + value;

    value = numberIsNaNTooManyArguments(2, 3, 5);
    if (value)
        throw "numberIsNaNTooManyArguments() = " + value;
}


// *** Constant as arguments. ***
function testNumberIsNaNOnConstants()
{
    var value = Number.isNaN(0);
    if (value)
        throw "Number.isNaN(0) = " + value;
    var value = Number.isNaN(-0);
    if (value)
        throw "Number.isNaN(-0) = " + value;
    var value = Number.isNaN(1);
    if (value)
        throw "Number.isNaN(1) = " + value;
    var value = Number.isNaN(-1);
    if (value)
        throw "Number.isNaN(-1) = " + value;
    var value = Number.isNaN(42);
    if (value)
        throw "Number.isNaN(42) = " + value;
    var value = Number.isNaN(-42);
    if (value)
        throw "Number.isNaN(-42) = " + value;
    var value = Number.isNaN(Number.POSITIVE_INFINITY);
    if (value)
        throw "Number.isNaN(Number.POSITIVE_INFINITY) = " + value;
    var value = Number.isNaN(Number.NEGATIVE_INFINITY);
    if (value)
        throw "Number.isNaN(Number.NEGATIVE_INFINITY) = " + value;
    var value = Number.isNaN(Math.E);
    if (value)
        throw "Number.isNaN(Math.E) = " + value;
    var value = Number.isNaN(NaN);
    if (!value)
        throw "Number.isNaN(NaN) = " + value;
}
noInline(testNumberIsNaNOnConstants);

for (var i = 0; i < 1e4; ++i) {
    testNumberIsNaNOnConstants();
}


// *** Struct transition. ***
function numberIsNaNStructTransition(value)
{
    return Number.isNaN(value);
}
noInline(numberIsNaNStructTransition);

for (var i = 0; i < 1e4; ++i) {
    var value = numberIsNaNStructTransition(42);
    if (value)
        throw "numberIsNaNStructTransition(42) = " + value;
}

Number.isNaN = function() { return 123; }

var value = numberIsNaNStructTransition(42);
if (value !== 123)
    throw "numberIsNaNStructTransition(42) after transition = " + value;
