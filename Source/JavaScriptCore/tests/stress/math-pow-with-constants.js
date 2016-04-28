function exponentIsZero(x) {
    return Math.pow(x, 0);
}
noInline(exponentIsZero);

function testExponentIsZero() {
    for (var i = 0; i < 10000; ++i) {
        var result = exponentIsZero(5);
        if (result !== 1)
            throw "Error: zeroExponent(5) should be 1, was = " + result;
    }
    for (var i = 0; i < 10000; ++i) {
        var result = exponentIsZero(5.5);
        if (result !== 1)
            throw "Error: zeroExponent(5.5) should be 1, was = " + result;
    }
}
testExponentIsZero();


function exponentIsOne(x) {
    return Math.pow(x, 1);
}
noInline(exponentIsOne);

function testExponentIsOne() {
    for (var i = 0; i < 10000; ++i) {
        var result = exponentIsOne(5);
        if (result !== 5)
            throw "Error: exponentIsOne(5) should be 5, was = " + result;
    }
    for (var i = 0; i < 10000; ++i) {
        var result = exponentIsOne(5.5);
        if (result !== 5.5)
            throw "Error: exponentIsOne(5.5) should be 5.5, was = " + result;
    }
}
testExponentIsOne();


function powUsedAsSqrt(x) {
    return Math.pow(x, 0.5);
}
noInline(powUsedAsSqrt);

function testPowUsedAsSqrt() {
    for (let i = 0; i < 1e4; ++i) {
        let result = powUsedAsSqrt(4);
        if (result !== Math.sqrt(4))
            throw "Error: powUsedAsSqrt(4) should be 2, was = " + result;
        result = powUsedAsSqrt(4.4);
        if (result !== Math.sqrt(4.4))
            throw "Error: powUsedAsSqrt(4) should be " + Math.sqrt(4.4) + ", was = " + result;
        if (powUsedAsSqrt(Infinity) !== Infinity)
            throw "Failed powUsedAsSqrt(Infinity)";
        if (powUsedAsSqrt(-Infinity) !== Infinity)
            throw "Failed powUsedAsSqrt(-Infinity)";
        let nanResult = powUsedAsSqrt(NaN)
        if (nanResult === nanResult)
            throw "Failed powUsedAsSqrt(NaN)";
        let zeroResult = powUsedAsSqrt(0.)
        if (zeroResult || (1 / zeroResult) !== Infinity)
            throw "Failed powUsedAsSqrt(0.)";
        let negativeZeroResult = powUsedAsSqrt(-0)
        if (negativeZeroResult || (1 / negativeZeroResult) !== Infinity)
            throw "Failed powUsedAsSqrt(-0)";
    }
}
testPowUsedAsSqrt();

function powUsedAsOneOverSqrt(x) {
    return Math.pow(x, -0.5);
}
noInline(powUsedAsOneOverSqrt);

function testPowUsedAsOneOverSqrt() {
    for (let i = 0; i < 1e4; ++i) {
        let result = powUsedAsOneOverSqrt(4);
        if (result !== 0.5)
            throw "Error: powUsedAsOneOverSqrt(4) should be 0.5, was = " + result;
        result = powUsedAsOneOverSqrt(4.4);
        if (result !== 1/Math.sqrt(4.4))
            throw "Error: powUsedAsOneOverSqrt(4) should be " + 1/Math.sqrt(4.4) + ", was = " + result;
        if (powUsedAsOneOverSqrt(Infinity) !== 0)
            throw "Failed powUsedAsOneOverSqrt(Infinity)";
        if (powUsedAsOneOverSqrt(-Infinity) !== 0)
            throw "Failed powUsedAsOneOverSqrt(-Infinity)";
        let nanResult = powUsedAsOneOverSqrt(NaN)
        if (nanResult === nanResult)
            throw "Failed powUsedAsOneOverSqrt(NaN)";
        if (powUsedAsOneOverSqrt(0) !== Infinity)
            throw "Failed powUsedAsOneOverSqrt(0)";
        if (powUsedAsOneOverSqrt(-0.) !== Infinity)
            throw "Failed powUsedAsOneOverSqrt(-0.)";
    }
}
testPowUsedAsOneOverSqrt();

function powUsedAsSquare(x) {
    return Math.pow(x, 2);
}
noInline(powUsedAsSquare);

function testPowUsedAsSquare() {
    for (let i = 0; i < 1e4; ++i) {
        let result = powUsedAsSquare(2);
        if (result !== 4)
            throw "Error: powUsedAsSquare(4) should be 2, was = " + result;
        result = powUsedAsSquare(4.4);
        if (result !== 19.360000000000003)
            throw "Error: powUsedAsSquare(4) should be " + 19.360000000000003 + ", was = " + result;
        result = powUsedAsSquare(Math.PI);
        if (result !== 9.869604401089358)
            throw "Error: powUsedAsSquare(4) should be " + 9.869604401089358 + ", was = " + result;
        if (powUsedAsSquare(Infinity) !== Infinity)
            throw "Failed powUsedAsSquare(Infinity)";
        if (powUsedAsSquare(-Infinity) !== Infinity)
            throw "Failed powUsedAsSquare(-Infinity)";
        let nanResult = powUsedAsSquare(NaN)
        if (nanResult === nanResult)
            throw "Failed powUsedAsSquare(NaN)";
        let zeroResult = powUsedAsSquare(0.)
        if (zeroResult || (1 / zeroResult) !== Infinity)
            throw "Failed powUsedAsSquare(0.)";
        let negativeZeroResult = powUsedAsSquare(-0)
        if (negativeZeroResult || (1 / negativeZeroResult) !== Infinity)
            throw "Failed powUsedAsSquare(-0)";
    }
}
testPowUsedAsSquare();

function intIntConstantsSmallNumbers() {
    return Math.pow(42, 3);
}
function intIntConstantsLargeNumbers() {
    // The result does not fit in a integer.
    return Math.pow(42, 42);
}
function intIntSmallConstants() {
    return Math.pow(42, 3);
}
function intDoubleConstants() {
    return Math.pow(14, 42.5);
}
function doubleDoubleConstants() {
    return Math.pow(13.5, 42.5);
}
function doubleIntConstants() {
    return Math.pow(13.5, 52);
}
noInline(intIntConstantsSmallNumbers);
noInline(intIntConstantsLargeNumbers);
noInline(intDoubleConstants);
noInline(doubleDoubleConstants);
noInline(doubleIntConstants);

function testBaseAndExponentConstantLiterals()
{
    for (var i = 0; i < 10000; ++i) {
        var result = intIntConstantsSmallNumbers();
        if (result !== 74088)
            throw "Error: intIntConstantsSmallNumbers() should be 74088, was = " + result;
    }
    for (var i = 0; i < 10000; ++i) {
        var result = intIntConstantsLargeNumbers();
        if (result !== 1.5013093754529656e+68)
            throw "Error: intIntConstantsLargeNumbers() should be 1.5013093754529656e+68, was = " + result;
    }
    for (var i = 0; i < 10000; ++i) {
        var result = intDoubleConstants();
        if (result !== 5.1338303882015765e+48)
            throw "Error: intDoubleConstants() should be 5.1338303882015765e+48, was = " + result;
    }
    for (var i = 0; i < 10000; ++i) {
        var result = doubleDoubleConstants();
        if (result !== 1.0944228729647829e+48)
            throw "Error: doubleDoubleConstants() should be 1.0944228729647829e+48, was = " + result;
    }
    for (var i = 0; i < 10000; ++i) {
        var result = doubleIntConstants();
        if (result !== 5.989022735311158e+58)
            throw "Error: doubleIntConstants() should be 5.989022735311158e+58, was = " + result;
    }
}
testBaseAndExponentConstantLiterals();


function exponentIsIntegerConstant(x) {
    return Math.pow(x, 42);
}
noInline(exponentIsIntegerConstant);

function testExponentIsIntegerConstant() {
    for (var i = 0; i < 1000; ++i) {
        var result = exponentIsIntegerConstant(2);
        if (result !== 4398046511104)
            throw "Error: exponentIsIntegerConstant(2) should be 4398046511104, was = " + result;
    }
    for (var i = 0; i < 1000; ++i) {
        var result = exponentIsIntegerConstant(5);
        if (result !== 2.2737367544323207e+29)
            throw "Error: exponentIsIntegerConstant(5) should be 2.2737367544323207e+29, was = " + result;
    }
    for (var i = 0; i < 1000; ++i) {
        var result = exponentIsIntegerConstant(2.1);
        if (result !== 34135823067412.42)
            throw "Error: exponentIsIntegerConstant(2.1) should be 34135823067412.42, was = " + result;
    }
}
testExponentIsIntegerConstant();


function exponentIsDoubleConstant(x) {
    return Math.pow(x, 42.5);
}
noInline(exponentIsDoubleConstant);

function testExponentIsDoubleConstant() {
    for (var i = 0; i < 1000; ++i) {
        var result = exponentIsDoubleConstant(2);
        if (result !== 6219777023950.95)
            throw "Error: exponentIsDoubleConstant(2) should be 6219777023950.95, was = " + result;
    }
    for (var i = 0; i < 1000; ++i) {
        var result = exponentIsDoubleConstant(5);
        if (result !== 5.084229945850415e+29)
            throw "Error: exponentIsDoubleConstant(5) should be 5.084229945850415e+29, was = " + result;
    }
    for (var i = 0; i < 1000; ++i) {
        var result = exponentIsDoubleConstant(2.1);
        if (result !== 49467507261113.805)
            throw "Error: exponentIsDoubleConstant(2.1) should be 49467507261113.805, was = " + result;
    }
}
testExponentIsDoubleConstant();


function exponentIsInfinityConstant(x) {
    return Math.pow(x, Infinity);
}
noInline(exponentIsInfinityConstant);

function testExponentIsInfinityConstant() {
    for (var i = 0; i < 1000; ++i) {
        var result = exponentIsInfinityConstant(2);
        if (result !== Infinity)
            throw "Error: exponentIsInfinityConstant(2) should be Infinity, was = " + result;
    }
    for (var i = 0; i < 1000; ++i) {
        var result = exponentIsInfinityConstant(5);
        if (result !== Infinity)
            throw "Error: exponentIsInfinityConstant(5) should be Infinity, was = " + result;
    }
    for (var i = 0; i < 1000; ++i) {
        var result = exponentIsInfinityConstant(2.1);
        if (result !== Infinity)
            throw "Error: exponentIsInfinityConstant(2.1) should be Infinity, was = " + result;
    }
}
testExponentIsInfinityConstant();


function exponentIsNegativeInfinityConstant(x) {
    return Math.pow(x, -Infinity);
}
noInline(exponentIsNegativeInfinityConstant);

function testExponentIsNegativeInfinityConstant() {
    for (var i = 0; i < 1000; ++i) {
        var result = exponentIsNegativeInfinityConstant(2);
        if (result !== 0)
            throw "Error: exponentIsNegativeInfinityConstant(2) should be zero, was = " + result;
    }
    for (var i = 0; i < 1000; ++i) {
        var result = exponentIsNegativeInfinityConstant(5);
        if (result !== 0)
            throw "Error: exponentIsNegativeInfinityConstant(5) should be zero, was = " + result;
    }
    for (var i = 0; i < 1000; ++i) {
        var result = exponentIsNegativeInfinityConstant(2.1);
        if (result !== 0)
            throw "Error: exponentIsNegativeInfinityConstant(2.1) should be zero, was = " + result;
    }
}
testExponentIsNegativeInfinityConstant();