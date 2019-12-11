function isNaNOnInteger(value)
{
    return isNaN(value);
}
noInline(isNaNOnInteger);

// *** Test simple cases on integers. ***
function testIsNaNOnIntegers()
{
    // Bounds.
    var value = isNaNOnInteger(0);
    if (value)
        throw "isNaNOnInteger(0) = " + value;

    var value = isNaNOnInteger(-2147483648);
    if (value)
        throw "isNaNOnInteger(-2147483648) = " + value;

    var value = isNaNOnInteger(2147483647);
    if (value)
        throw "isNaNOnInteger(2147483647) = " + value;

    // Simple values.
    var value = isNaNOnInteger(-1);
    if (value)
        throw "isNaNOnInteger(-1) = " + value;

    var value = isNaNOnInteger(42);
    if (value)
        throw "isNaNOnInteger(42) = " + value;

    var value = isNaNOnInteger(-42);
    if (value)
        throw "isNaNOnInteger(-42) = " + value;
}
noInline(testIsNaNOnIntegers);

for (var i = 0; i < 1e4; ++i) {
    testIsNaNOnIntegers();
}

// Make sure we don't do anything stupid when the type is unexpected.
function verifyIsNaNOnIntegerWithOtherTypes()
{
    var value = isNaNOnInteger(Math.PI);
    if (value)
        throw "isNaNOnInteger(Math.PI) = " + value;

    var value = isNaNOnInteger("42");
    if (value)
        throw "isNaNOnInteger(\"42\") = " + value;

    var value = isNaNOnInteger("WebKit");
    if (!value)
        throw "isNaNOnInteger(\"WebKit\") = " + value;

    var value = isNaNOnInteger(-0);
    if (value)
        throw "isNaNOnInteger(-0) = " + value;
}
noInline(verifyIsNaNOnIntegerWithOtherTypes);

for (var i = 0; i < 1e4; ++i) {
    verifyIsNaNOnIntegerWithOtherTypes();
}

// *** Test simple cases on doubles. ***
function isNaNOnDouble(value)
{
    return isNaN(value);
}
noInline(isNaNOnDouble);

// Test simple cases on doubles.
function testIsNaNOnDoubles()
{
    var value = isNaNOnDouble(Math.PI);
    if (value)
        throw "isNaNOnDouble(Math.PI) = " + value;

    var value = isNaNOnDouble(Math.E);
    if (value)
        throw "isNaNOnDouble(Math.E) = " + value;

    var value = isNaNOnDouble(Math.LN2);
    if (value)
        throw "isNaNOnDouble(Math.LN2) = " + value;

    var value = isNaNOnDouble(-0);
    if (value)
        throw "isNaNOnDouble(-0) = " + value;

    var value = isNaNOnDouble(NaN);
    if (!value)
        throw "isNaNOnDouble(NaN) = " + value;

    var value = isNaNOnDouble(Number.POSITIVE_INFINITY);
    if (value)
        throw "isNaNOnDouble(Number.POSITIVE_INFINITY) = " + value;

    var value = isNaNOnDouble(Number.NEGATIVE_INFINITY);
    if (value)
        throw "isNaNOnDouble(Number.NEGATIVE_INFINITY) = " + value;
}
noInline(testIsNaNOnDoubles);

for (var i = 0; i < 1e6; ++i) {
    testIsNaNOnDoubles();
}

// Make sure we don't do anything stupid when the type is unexpected.
function verifyIsNaNOnDoublesWithOtherTypes()
{
    var value = isNaNOnDouble(1);
    if (value)
        throw "isNaNOnDouble(1) = " + value;

    var value = isNaNOnDouble("42");
    if (value)
        throw "isNaNOnDouble(\"42\") = " + value;

    var value = isNaNOnDouble("WebKit");
    if (!value)
        throw "isNaNOnDouble(\"WebKit\") = " + value;

    var value = isNaNOnDouble({});
    if (!value)
        throw "isNaNOnDouble({}) = " + value;
}
noInline(verifyIsNaNOnDoublesWithOtherTypes);

for (var i = 0; i < 1e4; ++i) {
    verifyIsNaNOnDoublesWithOtherTypes();
}

// Make sure we still return NaN for type coerced values for global isNaN.
function verifyIsNaNOnCoercedTypes()
{
    var value = isNaNOnInteger("NaN");
    if (!value)
        throw "isNaNOnInteger(\"NaN\") = " + value;

    var value = isNaNOnInteger({ valueOf() { return NaN; } });
    if (!value)
        throw "isNaNOnInteger({ valueOf() { return NaN; } }) = " + value;
}
noInline(verifyIsNaNOnCoercedTypes);

for (var i = 0; i < 1e4; ++i) {
    verifyIsNaNOnCoercedTypes();
}


// *** Unusual arguments. ***
function isNaNNoArguments()
{
    return isNaN();
}
noInline(isNaNNoArguments);

function isNaNTooManyArguments(a, b, c)
{
    return isNaN(a, b, c);
}
noInline(isNaNTooManyArguments);


for (var i = 0; i < 1e4; ++i) {
    var value = isNaNNoArguments();
    if (!value)
        throw "isNaNNoArguments() = " + value;

    value = isNaNTooManyArguments(2, 3, 5);
    if (value)
        throw "isNaNTooManyArguments() = " + value;
}


// *** Constant as arguments. ***
function testIsNaNOnConstants()
{
    var value = isNaN(0);
    if (value)
        throw "isNaN(0) = " + value;
    var value = isNaN(-0);
    if (value)
        throw "isNaN(-0) = " + value;
    var value = isNaN(1);
    if (value)
        throw "isNaN(1) = " + value;
    var value = isNaN(-1);
    if (value)
        throw "isNaN(-1) = " + value;
    var value = isNaN(42);
    if (value)
        throw "isNaN(42) = " + value;
    var value = isNaN(-42);
    if (value)
        throw "isNaN(-42) = " + value;
    var value = isNaN(Number.POSITIVE_INFINITY);
    if (value)
        throw "isNaN(Number.POSITIVE_INFINITY) = " + value;
    var value = isNaN(Number.NEGATIVE_INFINITY);
    if (value)
        throw "isNaN(Number.NEGATIVE_INFINITY) = " + value;
    var value = isNaN(Math.E);
    if (value)
        throw "isNaN(Math.E) = " + value;
    var value = isNaN(NaN);
    if (!value)
        throw "isNaN(NaN) = " + value;
}
noInline(testIsNaNOnConstants);

for (var i = 0; i < 1e4; ++i) {
    testIsNaNOnConstants();
}


// *** Type Coercion Side effects. ***
function isNaNTypeCoercionSideEffects(value)
{
    return isNaN(value);
}
noInline(isNaNTypeCoercionSideEffects);

for (var i = 0; i < 1e4; ++i) {
    var value = isNaNTypeCoercionSideEffects(42);
    if (value)
        throw "isNaNTypeCoercionSideEffects(42) = " + value;
}

var globalCounter = 0;
for (var i = 0; i < 1e4; ++i) {
    var value = isNaNTypeCoercionSideEffects({ valueOf() { return globalCounter++; } });
    if (value)
        throw "isNaNTypeCoercionSideEffects({ valueOf() { return globalCounter++; } }) = " + value;
}
if (globalCounter !== 1e4)
    throw "globalCounter =" + globalCounter;


// *** Struct transition. ***
function isNaNStructTransition(value)
{
    return isNaN(value);
}
noInline(isNaNStructTransition);

for (var i = 0; i < 1e4; ++i) {
    var value = isNaNStructTransition(42);
    if (value)
        throw "isNaNStructTransition(42) = " + value;
}

isNaN = function() { return 123; }

var value = isNaNStructTransition(42);
if (value !== 123)
    throw "isNaNStructTransition(42) after transition = " + value;
