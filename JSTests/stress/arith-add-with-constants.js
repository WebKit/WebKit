// Test value + 0.
function arithAddIdentityWrittenAsInteger(x) {
    var a = x + 0;
    var b = 0 + x;
    if (!(isNaN(x) && isNaN(a) && isNaN(b)) && a !== b)
        throw "Internal error on arithAddIdentityWrittenAsInteger, a = " + a + " b = " + b;
    return a;
}
noInline(arithAddIdentityWrittenAsInteger);

function testArithAddIdentityWrittenAsInteger() {
    for (var i = 0; i < 1e4; ++i) {
        var result = arithAddIdentityWrittenAsInteger(i);
        if (result !== i) {
            throw "arithAddIdentityWrittenAsInteger(i) = " + result + ", expected " + i;
        }
    }

    for (var i = 0; i < 1e4; ++i) {
        var result = arithAddIdentityWrittenAsInteger(-0);
        if (result !== -0) {
            throw "arithAddIdentityWrittenAsInteger(-0) = " + result + ", expected -0";
        }
    }

    for (var i = 0; i < 1e4; ++i) {
        var testValue = i + .5;
        var result = arithAddIdentityWrittenAsInteger(testValue);
        if (result !== testValue) {
            throw "arithAddIdentityWrittenAsInteger(i) = " + result + ", expected " + testValue;
        }
    }

    for (var i = 0; i < 1e4; ++i) {;
        var result = arithAddIdentityWrittenAsInteger(NaN);
        if (!isNaN(result)) {
            throw "arithAddIdentityWrittenAsInteger(NaN) = " + result + ", expected NaN";
        }
    }

    for (var i = 0; i < 1e4; ++i) {;
        var result = arithAddIdentityWrittenAsInteger(Infinity);
        if (isFinite(result)) {
            throw "arithAddIdentityWrittenAsInteger(Infinity) = " + result + ", expected Infinity";
        }
    }

    for (var i = 0; i < 1e4; ++i) {;
        var result = arithAddIdentityWrittenAsInteger(-Infinity);
        if (isFinite(result) || result >= 0) {
            throw "arithAddIdentityWrittenAsInteger(-Infinity) = " + result + ", expected -Infinity";
        }
    }
}
testArithAddIdentityWrittenAsInteger();


function arithAddIdentityWrittenAsDouble(x) {
    var a = x + 0.0;
    var b = 0. + x;
    if (!(isNaN(x) && isNaN(a) && isNaN(b)) && a !== b)
        throw "Internal error on arithAddIdentityWrittenAsDouble, a = " + a + " b = " + b;
    return a;
}
noInline(arithAddIdentityWrittenAsDouble);

function testArithAddIdentityWrittenAsDouble() {
    for (var i = 0; i < 1e4; ++i) {
        var result = arithAddIdentityWrittenAsDouble(i);
        if (result !== i) {
            throw "arithAddIdentityWrittenAsDouble(i) = " + result + ", expected " + i;
        }
    }

    for (var i = 0; i < 1e4; ++i) {
        var result = arithAddIdentityWrittenAsDouble(-0);
        if (result !== -0) {
            throw "arithAddIdentityWrittenAsDouble(-0) = " + result + ", expected -0 ";
        }
    }

    for (var i = 0; i < 1e4; ++i) {
        var testValue = i + .5;
        var result = arithAddIdentityWrittenAsDouble(testValue);
        if (result !== testValue) {
            throw "arithAddIdentityWrittenAsDouble(i) = " + result + ", expected " + testValue;
        }
    }

    for (var i = 0; i < 1e4; ++i) {;
        var result = arithAddIdentityWrittenAsDouble(NaN);
        if (!isNaN(result)) {
            throw "arithAddIdentityWrittenAsDouble(NaN) = " + result + ", expected NaN";
        }
    }

    for (var i = 0; i < 1e4; ++i) {;
        var result = arithAddIdentityWrittenAsDouble(Infinity);
        if (isFinite(result)) {
            throw "arithAddIdentityWrittenAsDouble(Infinity) = " + result + ", expected Infinity";
        }
    }

    for (var i = 0; i < 1e4; ++i) {;
        var result = arithAddIdentityWrittenAsDouble(-Infinity);
        if (isFinite(result) || result >= 0) {
            throw "arithAddIdentityWrittenAsDouble(-Infinity) = " + result + ", expected -Infinity";
        }
    }
}
testArithAddIdentityWrittenAsDouble();


// Test "value + 42".
function arithAdd42WrittenAsInteger(x) {
    var a = x + 42;
    var b = 42 + x;
    if (!(isNaN(x) && isNaN(a) && isNaN(b)) && a !== b)
        throw "Internal error on arithAdd42WrittenAsInteger, a = " + a + " b = " + b;
    return a;
}
noInline(arithAdd42WrittenAsInteger);

function testArithAdd42WrittenAsInteger() {
    for (var i = 0; i < 1e4; ++i) {
        var result = arithAdd42WrittenAsInteger(13);
        if (result !== 55) {
            throw "arithAdd42WrittenAsInteger(13) = " + result + ", expected 55";
        }
    }

    for (var i = 0; i < 1e4; ++i) {
        var result = arithAdd42WrittenAsInteger(-0);
        if (result !== 42) {
            throw "arithAdd42WrittenAsInteger(-0) = " + result + ", expected 42";
        }
    }

    for (var i = 0; i < 1e4; ++i) {
        var result = arithAdd42WrittenAsInteger(13.3);
        if (result !== 55.3) {
            throw "arithAdd42WrittenAsInteger(13.3) = " + result + ", expected 55.3";
        }
    }

    for (var i = 0; i < 1e4; ++i) {;
        var result = arithAdd42WrittenAsInteger(NaN);
        if (!isNaN(result)) {
            throw "arithAdd42WrittenAsInteger(NaN) = " + result + ", expected NaN";
        }
    }

    for (var i = 0; i < 1e4; ++i) {;
        var result = arithAdd42WrittenAsInteger(Infinity);
        if (isFinite(result)) {
            throw "arithAdd42WrittenAsInteger(Infinity) = " + result + ", expected Infinity";
        }
    }

    for (var i = 0; i < 1e4; ++i) {;
        var result = arithAdd42WrittenAsInteger(-Infinity);
        if (isFinite(result) || result >= 0) {
            throw "arithAdd42WrittenAsInteger(-Infinity) = " + result + ", expected -Infinity";
        }
    }
}
testArithAdd42WrittenAsInteger();




// Test "value + 42".
function arithAdd42WrittenAsInteger(x) {
    var a = x + 42;
    var b = 42 + x;
    if (!(isNaN(x) && isNaN(a) && isNaN(b)) && a !== b)
        throw "Internal error on arithAdd42WrittenAsInteger, a = " + a + " b = " + b;
    return a;
}
noInline(arithAdd42WrittenAsInteger);

function testArithAdd42WrittenAsInteger() {
    for (var i = 0; i < 1e4; ++i) {
        var result = arithAdd42WrittenAsInteger(13);
        if (result !== 55) {
            throw "arithAdd42WrittenAsInteger(13) = " + result + ", expected 55";
        }
    }

    for (var i = 0; i < 1e4; ++i) {
        var result = arithAdd42WrittenAsInteger(-0);
        if (result !== 42) {
            throw "arithAdd42WrittenAsInteger(-0) = " + result + ", expected 42";
        }
    }

    for (var i = 0; i < 1e4; ++i) {
        var result = arithAdd42WrittenAsInteger(13.3);
        if (result !== 55.3) {
            throw "arithAdd42WrittenAsInteger(13.3) = " + result + ", expected 55.3";
        }
    }

    for (var i = 0; i < 1e4; ++i) {;
        var result = arithAdd42WrittenAsInteger(NaN);
        if (!isNaN(result)) {
            throw "arithAdd42WrittenAsInteger(NaN) = " + result + ", expected NaN";
        }
    }

    for (var i = 0; i < 1e4; ++i) {;
        var result = arithAdd42WrittenAsInteger(Infinity);
        if (isFinite(result)) {
            throw "arithAdd42WrittenAsInteger(Infinity) = " + result + ", expected Infinity";
        }
    }

    for (var i = 0; i < 1e4; ++i) {;
        var result = arithAdd42WrittenAsInteger(-Infinity);
        if (isFinite(result) || result >= 0) {
            throw "arithAdd42WrittenAsInteger(-Infinity) = " + result + ", expected -Infinity";
        }
    }
}
testArithAdd42WrittenAsInteger();

function arithSub42WrittenAsDouble(x) {
    var a = (x|0) - 42.0;
    var b = -42. + (x|0);
    if (!(isNaN(x) && isNaN(a) && isNaN(b)) && a !== b)
        throw "Internal error on arithSub42WrittenAsDouble, a = " + a + " b = " + b;
    return a;
}
noInline(arithSub42WrittenAsDouble);

function testArithSub42WrittenAsDouble() {
    for (var i = 0; i < 1e4; ++i) {
        var result = arithSub42WrittenAsDouble(13);
        if (result !== -29) {
            throw "arithSub42WrittenAsDouble(13) = " + result + ", expected -29";
        }
    }

    for (var i = 0; i < 1e4; ++i) {
        var result = arithSub42WrittenAsDouble(-0);
        if (result !== -42) {
            throw "arithSub42WrittenAsDouble(-0) = " + result + ", expected -42";
        }
    }

    for (var i = 0; i < 1e4; ++i) {
        var result = arithSub42WrittenAsDouble(13.3);
        if (result !== -29) {
            throw "arithSub42WrittenAsDouble(13.3) = " + result + ", expected -29";
        }
    }
}
testArithSub42WrittenAsDouble();


function doubleConstant(){
    Math.min(0.0);
    +0.0;
} noInline(doubleConstant);

function testDoubleConstant(){
    for (var i = 0; i < 1e4; ++i) {
        doubleConstant();
    }
}
testDoubleConstant();
