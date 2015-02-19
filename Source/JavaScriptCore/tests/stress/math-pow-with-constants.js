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
    for (var i = 0; i < 10000; ++i) {
        var result = powUsedAsSqrt(4);
        if (result !== Math.sqrt(4))
            throw "Error: powUsedAsSqrt(4) should be 2, was = " + result;
    }
    for (var i = 0; i < 10000; ++i) {
        var result = powUsedAsSqrt(4.4);
        if (result !== Math.sqrt(4.4))
            throw "Error: powUsedAsSqrt(4) should be " + Math.sqrt(4.4) + ", was = " + result;
    }

}
testPowUsedAsSqrt();


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