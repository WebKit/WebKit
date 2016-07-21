// If an argument is NaN, the result of x ** y is NaN.
function testIntegerBaseWithNaNExponentStatic() {
    for (var i = 0; i < 10000; ++i) {
        var result = 5 ** NaN;
        if (!isNaN(result))
            throw "Error: bad result, 5 ** NaN = " + result;
    }
    for (var i = 0; i < 10000; ++i) {
        var result = i ** NaN;
        if (!isNaN(result))
            throw "Error: bad result, i ** NaN = " + result + " with i = " + i;
    }
}
noInline(testIntegerBaseWithNaNExponentStatic);
testIntegerBaseWithNaNExponentStatic();

function mathPowIntegerBaseWithNaNExponentDynamic(x, y) {
    return x ** y;
}
noInline(mathPowIntegerBaseWithNaNExponentDynamic);
function testIntegerBaseWithNaNExponentDynamic() {
    // Warm up with 2 integers.
    for (var i = 0; i < 10000; ++i) {
        var result = mathPowIntegerBaseWithNaNExponentDynamic(2, 5);
        if (result !== 32)
            throw "Error: bad result, mathPowIntegerBaseWithNaNExponentDynamic(2, 5) = " + result + ", expected 32."
    }

    for (var i = 0; i < 10000; ++i) {
        var result = mathPowIntegerBaseWithNaNExponentDynamic(i, NaN);
        if (!isNaN(result))
            throw "Error: bad result, mathPowIntegerBaseWithNaNExponentDynamic(i, NaN) = " + result + " with i = " + i + ", expected NaN";
    }
}
noInline(testIntegerBaseWithNaNExponentDynamic);
testIntegerBaseWithNaNExponentDynamic();

function testFloatingPointBaseWithNaNExponentStatic() {
    for (var i = 0; i < 10000; ++i) {
        var result = 5.5 ** NaN;
        if (!isNaN(result))
            throw "Error: bad result, 5.5 ** NaN = " + result;
    }
    for (var i = 0; i < 10000; ++i) {
        var result = (i + 0.5) ** NaN;
        if (!isNaN(result))
            throw "Error: bad result, (i + 0.5) ** NaN = " + result + " with i = " + i;
    }
}
noInline(testFloatingPointBaseWithNaNExponentStatic);
testFloatingPointBaseWithNaNExponentStatic();

function mathPowFloatingPointBaseWithNaNExponentDynamic(x, y) {
    return x ** y;
}
noInline(mathPowFloatingPointBaseWithNaNExponentDynamic);
function testFloatingPointBaseWithNaNExponentDynamic() {
    // Warm up with 2 double.
    for (var i = 0; i < 10000; ++i) {
        var result = mathPowFloatingPointBaseWithNaNExponentDynamic(2.5, 5.1);
        if (result !== 107.02717054543135)
            throw "Error: bad result, mathPowFloatingPointBaseWithNaNExponentDynamic(2.5, 5.1) = " + result + ", expected 107.02717054543135."
    }

    for (var i = 0; i < 10000; ++i) {
        var result = mathPowFloatingPointBaseWithNaNExponentDynamic(i + 0.5, NaN);
        if (!isNaN(result))
            throw "Error: bad result, mathPowFloatingPointBaseWithNaNExponentDynamic(i + 0.5, NaN) = " + result + " with i = " + i + ", expected NaN";
    }
}
noInline(testFloatingPointBaseWithNaNExponentDynamic);
testFloatingPointBaseWithNaNExponentDynamic();

// If y is +0, the result is 1, even if x is NaN.
// If y is -0, the result is 1, even if x is NaN.
// If x is NaN and y is nonzero, the result is NaN.
function testNaNBaseStatic() {
    for (var i = 0; i < 10000; ++i) {
        var result = NaN ** (i + 1);
        if (!isNaN(result))
            throw "Error: bad result, NaN ** (i + 1) = " + result + " with i = " + i;
    }
    for (var i = 0; i < 10000; ++i) {
        var result = NaN ** (i + 1.5);
        if (!isNaN(result))
            throw "Error: bad result, NaN ** (i + 1.5) = " + result + " with i = " + i;
    }
    for (var i = 0; i < 10000; ++i) {
        var result = NaN ** 0;
        if (result !== 1)
            throw "Error: bad result, NaN ** 0 = " + result;
    }
    for (var i = 0; i < 10000; ++i) {
        var result = NaN ** -0;
        if (result !== 1)
            throw "Error: bad result, NaN ** -0 = " + result;
    }
}
noInline(testNaNBaseStatic);
testNaNBaseStatic();

function mathPowNaNBaseDynamic1(x, y) {
    return x ** y;
}
function mathPowNaNBaseDynamic2(x, y) {
    return x ** y;
}
function mathPowNaNBaseDynamic3(x, y) {
    return x ** y;
}
function mathPowNaNBaseDynamic4(x, y) {
    return x ** y;
}
noInline(mathPowNaNBaseDynamic1);
noInline(mathPowNaNBaseDynamic2);
noInline(mathPowNaNBaseDynamic3);
noInline(mathPowNaNBaseDynamic4);
function testNaNBaseDynamic() {
    for (var i = 0; i < 10000; ++i) {
        var result = mathPowNaNBaseDynamic1(NaN, i + 1);
        if (!isNaN(result))
            throw "Error: bad result, mathPowNaNBaseDynamic1(NaN, i + 1) = " + result + " with i = " + i;
    }
    for (var i = 0; i < 10000; ++i) {
        var result = mathPowNaNBaseDynamic2(NaN, i + 1.5);
        if (!isNaN(result))
            throw "Error: bad result, mathPowNaNBaseDynamic2(NaN, i + 1.5) = " + result + " with i = " + i;
    }
    for (var i = 0; i < 10000; ++i) {
        var result = mathPowNaNBaseDynamic3(NaN, 0);
        if (result !== 1)
            throw "Error: bad result, mathPowNaNBaseDynamic3(NaN, 0) = " + result;
    }
    for (var i = 0; i < 10000; ++i) {
        var result = mathPowNaNBaseDynamic4(NaN, -0);
        if (result !== 1)
            throw "Error: bad result, mathPowNaNBaseDynamic4(NaN, -0) = " + result;
    }
}
noInline(testNaNBaseDynamic);
testNaNBaseDynamic();

// If abs(x) is 1 and y is +Inf the result is NaN.
// If abs(x) is 1 and y is −Inf the result is NaN.
function infiniteExponentsStatic() {
    for (var i = 0; i < 10000; ++i) {
        var result = 1 ** Number.POSITIVE_INFINITY;
        if (!isNaN(result))
            throw "Error: bad result, 1 ** Number.POSITIVE_INFINITY = " + result;
    }
    for (var i = 0; i < 10000; ++i) {
        var result = (-1) ** Number.POSITIVE_INFINITY;
        if (!isNaN(result))
            throw "Error: bad result, -1 ** Number.POSITIVE_INFINITY = " + result;
    }
    for (var i = 0; i < 10000; ++i) {
        var result = 1 ** Number.NEGATIVE_INFINITY;
        if (!isNaN(result))
            throw "Error: bad result, 1 ** Number.NEGATIVE_INFINITY = " + result;
    }
    for (var i = 0; i < 10000; ++i) {
        var result = (-1) ** Number.NEGATIVE_INFINITY;
        if (!isNaN(result))
            throw "Error: bad result, -1 ** Number.NEGATIVE_INFINITY = " + result;
    }
}
noInline(infiniteExponentsStatic);
infiniteExponentsStatic();

function mathPowInfiniteExponentsDynamic1(x, y) {
    return x ** y;
}
function mathPowInfiniteExponentsDynamic2(x, y) {
    return x ** y;
}
function mathPowInfiniteExponentsDynamic3(x, y) {
    return x ** y;
}
function mathPowInfiniteExponentsDynamic4(x, y) {
    return x ** y;
}
noInline(mathPowInfiniteExponentsDynamic1);
noInline(mathPowInfiniteExponentsDynamic2);
noInline(mathPowInfiniteExponentsDynamic3);
noInline(mathPowInfiniteExponentsDynamic4);
function infiniteExponentsDynamic() {
    for (var i = 0; i < 10000; ++i) {
        var result = mathPowInfiniteExponentsDynamic1(1, Number.POSITIVE_INFINITY);
        if (!isNaN(result))
            throw "Error: bad result, mathPowInfiniteExponentsDynamic1(1, Number.POSITIVE_INFINITY) = " + result;
    }
    for (var i = 0; i < 10000; ++i) {
        var result = mathPowInfiniteExponentsDynamic2(-1, Number.POSITIVE_INFINITY);
        if (!isNaN(result))
            throw "Error: bad result, mathPowInfiniteExponentsDynamic2(-1, Number.POSITIVE_INFINITY) = " + result;
    }
    for (var i = 0; i < 10000; ++i) {
        var result = mathPowInfiniteExponentsDynamic3(1, Number.NEGATIVE_INFINITY);
        if (!isNaN(result))
            throw "Error: bad result, mathPowInfiniteExponentsDynamic3(1, Number.NEGATIVE_INFINITY) = " + result;
    }
    for (var i = 0; i < 10000; ++i) {
        var result = mathPowInfiniteExponentsDynamic4(-1, Number.NEGATIVE_INFINITY);
        if (!isNaN(result))
            throw "Error: bad result, mathPowInfiniteExponentsDynamic4(-1, Number.NEGATIVE_INFINITY) = " + result;
    }
}
noInline(infiniteExponentsDynamic);
infiniteExponentsDynamic();
