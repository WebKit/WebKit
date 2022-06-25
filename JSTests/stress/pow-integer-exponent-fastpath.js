function valuesAreClose(a, b) {
    return Math.abs(a / b) - 1 < 1e-10;
}

// Small exponent values are handled through a simpler inline loop. Test that it is not observable.
function mathPowDoubleDoubleTestExponentFifty(x, y) {
    return x ** y
}
noInline(mathPowDoubleDoubleTestExponentFifty);

function mathPowDoubleIntTestExponentFifty(x, y) {
    return x ** y
}
noInline(mathPowDoubleIntTestExponentFifty);
function testExponentFifty(x, y, expected) {
    for (var i = 0; i < 10000; ++i) {
        var result = mathPowDoubleDoubleTestExponentFifty(x, y);
        if (!valuesAreClose(result, expected))
            throw "Error: bad result, (" + x + ") ** (" + y + ") = " + result + " expected value close to " + expected;
    }
    var integerY = y | 0;
    for (var i = 0; i < 10000; ++i) {
        var result = mathPowDoubleIntTestExponentFifty(x, integerY);
        if (!valuesAreClose(result, expected))
            throw "Error: bad result, (" + x + ") ** (" + integerY + ") = " + result + " expected value close to " + expected;
    }
}
noInline(testExponentFifty);
testExponentFifty(53.70901164133102, 50.0, 3.179494118120144e+86);
testExponentFifty(53.70901164133102, -10.0, 5.006432842621192e-18);

function mathPowDoubleDoubleTestExponentTenThousands(x, y) {
    return x ** y
}
noInline(mathPowDoubleDoubleTestExponentTenThousands);

function mathPowDoubleIntTestExponentTenThousands(x, y) {
    return x ** y
}
noInline(mathPowDoubleIntTestExponentTenThousands);
function testExponentTenThousands(x, y, expected) {
    for (var i = 0; i < 10000; ++i) {
        var result = mathPowDoubleDoubleTestExponentTenThousands(x, y);
        if (!valuesAreClose(result, expected))
            throw "Error: bad result, (" + x + ") ** (" + y + ") = " + result + " expected value close to " + expected;
    }
    var integerY = y | 0;
    for (var i = 0; i < 10000; ++i) {
        var result = mathPowDoubleIntTestExponentTenThousands(x, integerY);
        if (!valuesAreClose(result, expected))
            throw "Error: bad result, (" + x + ") ** (" + integerY + ") = " + result + " expected value close to " + expected;
    }
}
noInline(testExponentTenThousands);
testExponentTenThousands(1.001, 10000.0, 21916.681339048373);
testExponentTenThousands(1.001, -1.0, 0.9990009990009991);
