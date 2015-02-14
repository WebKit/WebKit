// If y is NaN, the result is NaN.
function testIntegerBaseWithNaNExponent() {
    for (var i = 0; i < 10000; ++i) {
        var result = Math.pow(5, NaN);
        if (!isNaN(result))
            throw "Error: bad result, Math.pow(5, NaN) = " + result;
    }
    for (var i = 0; i < 10000; ++i) {
        var result = Math.pow(i, NaN);
        if (!isNaN(result))
            throw "Error: bad result, Math.pow(i, NaN) = " + result + " with i = " + i;
    }
}
noInline(testIntegerBaseWithNaNExponent);
testIntegerBaseWithNaNExponent();

function testFloatingPointBaseWithNaNExponent() {
    for (var i = 0; i < 10000; ++i) {
        var result = Math.pow(5.5, NaN);
        if (!isNaN(result))
            throw "Error: bad result, Math.pow(5.5, NaN) = " + result;
    }
    for (var i = 0; i < 10000; ++i) {
        var result = Math.pow(i + 1, NaN);
        if (!isNaN(result))
            throw "Error: bad result, Math.pow(i + 0.5, NaN) = " + result + " with i = " + i;
    }
}
noInline(testFloatingPointBaseWithNaNExponent);
testFloatingPointBaseWithNaNExponent();

// If y is +0, the result is 1, even if x is NaN.
// If y is −0, the result is 1, even if x is NaN.
// If x is NaN and y is nonzero, the result is NaN.
function testNaNBase() {
    for (var i = 0; i < 10000; ++i) {
        var result = Math.pow(NaN, i + 1);
        if (!isNaN(result))
            throw "Error: bad result, Math.pow(NaN, i + 1) = " + result + " with i = " + i;
    }
    for (var i = 0; i < 10000; ++i) {
        var result = Math.pow(NaN, i + 1.5);
        if (!isNaN(result))
            throw "Error: bad result, Math.pow(NaN, i + 1.5) = " + result + " with i = " + i;
    }
    for (var i = 0; i < 10000; ++i) {
        var result = Math.pow(NaN, 0);
        if (result !== 1)
            throw "Error: bad result, Math.pow(NaN, 0) = " + result;
    }
    for (var i = 0; i < 10000; ++i) {
        var result = Math.pow(NaN, -0);
        if (result !== 1)
            throw "Error: bad result, Math.pow(NaN, -0) = " + result;
    }
}
noInline(testNaNBase);
testNaNBase();

// If abs(x) is 1 and y is +∞, the result is NaN.
// If abs(x) is 1 and y is −∞, the result is NaN.
function infiniteExponents() {
    for (var i = 0; i < 10000; ++i) {
        var result = Math.pow(1, Number.POSITIVE_INFINITY);
        if (!isNaN(result))
            throw "Error: bad result, Math.pow(1, Number.POSITIVE_INFINITY) = " + result;
    }
    for (var i = 0; i < 10000; ++i) {
        var result = Math.pow(-1, Number.POSITIVE_INFINITY);
        if (!isNaN(result))
            throw "Error: bad result, Math.pow(-1, Number.POSITIVE_INFINITY) = " + result;
    }
    for (var i = 0; i < 10000; ++i) {
        var result = Math.pow(1, Number.NEGATIVE_INFINITY);
        if (!isNaN(result))
            throw "Error: bad result, Math.pow(1, Number.NEGATIVE_INFINITY) = " + result;
    }
    for (var i = 0; i < 10000; ++i) {
        var result = Math.pow(-1, Number.NEGATIVE_INFINITY);
        if (!isNaN(result))
            throw "Error: bad result, Math.pow(-1, Number.NEGATIVE_INFINITY) = " + result;
    }
}
noInline(infiniteExponents);
infiniteExponents();
