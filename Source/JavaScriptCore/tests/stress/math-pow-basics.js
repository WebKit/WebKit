function valuesAreClose(a, b) {
    return Math.abs(a / b) - 1 < 1e-10;
}

// Some random values.
function mathPowDoubleDouble1(x, y) {
    return Math.pow(x, y)
}
noInline(mathPowDoubleDouble1);

function mathPowDoubleInt1(x, y) {
    return Math.pow(x, y)
}
noInline(mathPowDoubleInt1);

function test1(x, y, expected1, expected2) {
    for (var i = 0; i < 10000; ++i) {
        var result = mathPowDoubleDouble1(x, y);
        if (!valuesAreClose(result, expected1))
            throw "Error: bad result, mathPowDoubleDouble1(" + x + ", " + y + ") = " + result + " expected a value close to " + expected1;
    }
    var integerY = y | 0;
    for (var i = 0; i < 10000; ++i) {
        var result = mathPowDoubleInt1(x, integerY);
        if (!valuesAreClose(result, expected2))
            throw "Error: bad result, mathPowDoubleInt1(" + x + ", " + integerY + ") = " + result + " expected a value close to " + expected2;
    }
}
noInline(test1);
test1(376.76522764377296, 10.81699226051569, 7.333951929109252e+27, 5.76378989575089e+25);

function mathPowDoubleDouble2(x, y) {
    return Math.pow(x, y)
}
noInline(mathPowDoubleDouble2);

function mathPowDoubleInt2(x, y) {
    return Math.pow(x, y)
}
noInline(mathPowDoubleInt2);
function test2(x, y, expected1, expected2) {
    for (var i = 0; i < 10000; ++i) {
        var result = mathPowDoubleDouble2(x, y);
        if (!valuesAreClose(result, expected1))
            throw "Error: bad result, mathPowDoubleDouble2(" + x + ", " + y + ") = " + result + " expected a value close to " + expected1;
    }
    var integerY = y | 0;
    for (var i = 0; i < 10000; ++i) {
        var result = mathPowDoubleInt2(x, integerY);
        if (!valuesAreClose(result, expected2))
            throw "Error: bad result, mathPowDoubleInt2(" + x + ", " + integerY + ") = " + result + " expected a value close to " + expected2;
    }
}
noInline(test2);
test2(376.76522764377296, -5.81699226051569, 1.035180331187579e-15, 1.3171824310400265e-13);

function mathPowDoubleDouble3(x, y) {
    return Math.pow(x, y)
}
noInline(mathPowDoubleDouble3);

function mathPowDoubleInt3(x, y) {
    return Math.pow(x, y)
}
noInline(mathPowDoubleInt3);
function test3(x, y, expected1, expected2) {
    for (var i = 0; i < 10000; ++i) {
        var result = mathPowDoubleDouble3(x, y);
        if (!valuesAreClose(result, expected1))
            throw "Error: bad result, mathPowDoubleDouble3(" + x + ", " + y + ") = " + result + " expected a value close to " + expected1;
    }
    var integerY = y | 0;
    for (var i = 0; i < 10000; ++i) {
        var result = mathPowDoubleInt3(x, integerY);
        if (!valuesAreClose(result, expected2))
            throw "Error: bad result, mathPowDoubleInt3(" + x + ", " + integerY + ") = " + result + " expected a value close to " + expected2;
    }
}
noInline(test3);
test3(-37.676522764377296, 10.0, 5763789895750892, 5763789895750892);

// Exponent zero.
function mathPowDoubleDouble4(x, y) {
    return Math.pow(x, y)
}
noInline(mathPowDoubleDouble4);

function mathPowDoubleInt4(x, y) {
    return Math.pow(x, y)
}
noInline(mathPowDoubleInt4);
function test4(x, y, expected1, expected2) {
    for (var i = 0; i < 10000; ++i) {
        var result = mathPowDoubleDouble4(x, y);
        if (!valuesAreClose(result, expected1))
            throw "Error: bad result, mathPowDoubleDouble4(" + x + ", " + y + ") = " + result + " expected a value close to " + expected1;
    }
    var integerY = y | 0;
    for (var i = 0; i < 10000; ++i) {
        var result = mathPowDoubleInt4(x, integerY);
        if (!valuesAreClose(result, expected2))
            throw "Error: bad result, mathPowDoubleInt4(" + x + ", " + integerY + ") = " + result + " expected a value close to " + expected2;
    }
}
noInline(test4);
test4(-37.676522764377296, 0, 1, 1);

// Exponent minus zero.
function mathPowDoubleDouble5(x, y) {
    return Math.pow(x, y)
}
noInline(mathPowDoubleDouble5);

function mathPowDoubleInt5(x, y) {
    return Math.pow(x, y)
}
noInline(mathPowDoubleInt5);
function test5(x, y, expected1, expected2) {
    for (var i = 0; i < 10000; ++i) {
        var result = mathPowDoubleDouble5(x, y);
        if (!valuesAreClose(result, expected1))
            throw "Error: bad result, mathPowDoubleDouble5(" + x + ", " + y + ") = " + result + " expected a value close to " + expected1;
    }
    var integerY = y | 0;
    for (var i = 0; i < 10000; ++i) {
        var result = mathPowDoubleInt5(x, integerY);
        if (!valuesAreClose(result, expected2))
            throw "Error: bad result, mathPowDoubleInt(" + x + ", " + integerY + ") = " + result + " expected a value close to " + expected2;
    }
}
noInline(test5);
test5(-37.676522764377296, -0, 1, 1);

// Exponent 1.
function mathPowDoubleDouble6(x, y) {
    return Math.pow(x, y)
}
noInline(mathPowDoubleDouble6);

function mathPowDoubleInt6(x, y) {
    return Math.pow(x, y)
}
noInline(mathPowDoubleInt6);
function test6(x, y, expected1, expected2) {
    for (var i = 0; i < 10000; ++i) {
        var result = mathPowDoubleDouble6(x, y);
        if (!valuesAreClose(result, expected1))
            throw "Error: bad result, mathPowDoubleDouble6(" + x + ", " + y + ") = " + result + " expected a value close to " + expected1;
    }
    var integerY = y | 0;
    for (var i = 0; i < 10000; ++i) {
        var result = mathPowDoubleInt6(x, integerY);
        if (!valuesAreClose(result, expected2))
            throw "Error: bad result, mathPowDoubleInt6(" + x + ", " + integerY + ") = " + result + " expected a value close to " + expected2;
    }
}
noInline(test6);
test6(-37.676522764377296, 1.0, -37.676522764377296, -37.676522764377296);

// Exponent -1.
function mathPowDoubleDouble7(x, y) {
    return Math.pow(x, y)
}
noInline(mathPowDoubleDouble7);

function mathPowDoubleInt7(x, y) {
    return Math.pow(x, y)
}
noInline(mathPowDoubleInt7);
function test7(x, y, expected1, expected2) {
    for (var i = 0; i < 10000; ++i) {
        var result = mathPowDoubleDouble7(x, y);
        if (!valuesAreClose(result, expected1))
            throw "Error: bad result, mathPowDoubleDouble7(" + x + ", " + y + ") = " + result + " expected a value close to " + expected1;
    }
    var integerY = y | 0;
    for (var i = 0; i < 10000; ++i) {
        var result = mathPowDoubleDouble7(x, integerY);
        if (!valuesAreClose(result, expected2))
            throw "Error: bad result, mathPowDoubleDouble7(" + x + ", " + integerY + ") = " + result + " expected a value close to " + expected2;
    }
}
noInline(test7);
test7(-37.676522764377296, -1.0, -0.026541727490454296, -0.026541727490454296);

// Let's square things.
function mathPowDoubleDouble8(x, y) {
    return Math.pow(x, y)
}
noInline(mathPowDoubleDouble8);

function mathPowDoubleInt8(x, y) {
    return Math.pow(x, y)
}
noInline(mathPowDoubleInt8);
function test8(x, y, expected1, expected2) {
    for (var i = 0; i < 10000; ++i) {
        var result = mathPowDoubleDouble8(x, y);
        if (!valuesAreClose(result, expected1))
            throw "Error: bad result, mathPowDoubleDouble8(" + x + ", " + y + ") = " + result + " expected a value close to " + expected1;
    }
    var integerY = y | 0;
    for (var i = 0; i < 10000; ++i) {
        var result = mathPowDoubleInt8(x, integerY);
        if (!valuesAreClose(result, expected2))
            throw "Error: bad result, mathPowDoubleInt8(" + x + ", " + integerY + ") = " + result + " expected a value close to " + expected2;
    }
}
noInline(test8);
test8(-37.676522764377296, 2.0, 1419.5203676146407, 1419.5203676146407);

function mathPowDoubleDouble9(x, y) {
    return Math.pow(x, y)
}
noInline(mathPowDoubleDouble9);

function mathPowDoubleInt9(x, y) {
    return Math.pow(x, y)
}
noInline(mathPowDoubleInt9);
function test9(x, y, expected1, expected2) {
    for (var i = 0; i < 10000; ++i) {
        var result = mathPowDoubleDouble9(x, y);
        if (!valuesAreClose(result, expected1))
            throw "Error: bad result, mathPowDoubleDouble9(" + x + ", " + y + ") = " + result + " expected a value close to " + expected1;
    }
    var integerY = y | 0;
    for (var i = 0; i < 10000; ++i) {
        var result = mathPowDoubleInt9(x, integerY);
        if (!valuesAreClose(result, expected2))
            throw "Error: bad result, mathPowDoubleInt9(" + x + ", " + integerY + ") = " + result + " expected a value close to " + expected2;
    }
}
noInline(test9);
test9(37.676522764377296, 2.0, 1419.5203676146407, 1419.5203676146407);

// Let's cube things.
function mathPowDoubleDouble10(x, y) {
    return Math.pow(x, y)
}
noInline(mathPowDoubleDouble10);

function mathPowDoubleInt10(x, y) {
    return Math.pow(x, y)
}
noInline(mathPowDoubleInt10);
function test10(x, y, expected1, expected2) {
    for (var i = 0; i < 10000; ++i) {
        var result = mathPowDoubleDouble10(x, y);
        if (!valuesAreClose(result, expected1))
            throw "Error: bad result, mathPowDoubleDouble(" + x + ", " + y + ") = " + result + " expected a value close to " + expected1;
    }
    var integerY = y | 0;
    for (var i = 0; i < 10000; ++i) {
        var result = mathPowDoubleInt10(x, integerY);
        if (!valuesAreClose(result, expected2))
            throw "Error: bad result, mathPowDoubleInt(" + x + ", " + integerY + ") = " + result + " expected a value close to " + expected2;
    }
}
noInline(test10);
test10(-37.676522764377296, 3.0, -53482.591444930236, -53482.591444930236);

function mathPowDoubleDouble11(x, y) {
    return Math.pow(x, y)
}
noInline(mathPowDoubleDouble11);

function mathPowDoubleInt11(x, y) {
    return Math.pow(x, y)
}
noInline(mathPowDoubleInt11);
function test11(x, y, expected1, expected2) {
    for (var i = 0; i < 10000; ++i) {
        var result = mathPowDoubleDouble11(x, y);
        if (!valuesAreClose(result, expected1))
            throw "Error: bad result, mathPowDoubleDouble(" + x + ", " + y + ") = " + result + " expected a value close to " + expected1;
    }
    var integerY = y | 0;
    for (var i = 0; i < 10000; ++i) {
        var result = mathPowDoubleInt11(x, integerY);
        if (!valuesAreClose(result, expected2))
            throw "Error: bad result, mathPowDoubleInt(" + x + ", " + integerY + ") = " + result + " expected a value close to " + expected2;
    }
}
noInline(test11);
test11(37.676522764377296, 3.0, 53482.591444930236, 53482.591444930236);
