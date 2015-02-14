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
