// Test value * 1.
function arithMulIdentityWrittenAsInteger(x) {
    var a = x * 1;
    var b = 1 * x;
    if (!(isNaN(x) && isNaN(a) && isNaN(b)) && a !== b)
        throw "Internal error on arithMulIdentityWrittenAsInteger, a = " + a + " b = " + b;
    return a;
}
noInline(arithMulIdentityWrittenAsInteger);

function testArithMulIdentityWrittenAsInteger() {
    for (var i = 0; i < 1e4; ++i) {
        var result = arithMulIdentityWrittenAsInteger(i);
        if (result !== i) {
            throw "arithMulIdentityWrittenAsInteger(i) = " + result + ", expected " + i;
        }
    }

    for (var i = 0; i < 1e4; ++i) {
        var result = arithMulIdentityWrittenAsInteger(-0);
        if (result !== -0) {
            throw "arithMulIdentityWrittenAsInteger(-0) = " + result + ", expected -0";
        }
    }

    for (var i = 0; i < 1e4; ++i) {
        var testValue = i + .5;
        var result = arithMulIdentityWrittenAsInteger(testValue);
        if (result !== testValue) {
            throw "arithMulIdentityWrittenAsInteger(i) = " + result + ", expected " + testValue;
        }
    }

    for (var i = 0; i < 1e4; ++i) {;
        var result = arithMulIdentityWrittenAsInteger(NaN);
        if (!isNaN(result)) {
            throw "arithMulIdentityWrittenAsInteger(NaN) = " + result + ", expected NaN";
        }
    }

    for (var i = 0; i < 1e4; ++i) {;
        var result = arithMulIdentityWrittenAsInteger(Infinity);
        if (isFinite(result)) {
            throw "arithMulIdentityWrittenAsInteger(Infinity) = " + result + ", expected Infinity";
        }
    }

    for (var i = 0; i < 1e4; ++i) {;
        var result = arithMulIdentityWrittenAsInteger(-Infinity);
        if (isFinite(result) || result >= 0) {
            throw "arithMulIdentityWrittenAsInteger(-Infinity) = " + result + ", expected -Infinity";
        }
    }
}
testArithMulIdentityWrittenAsInteger();


function arithMulIdentityWrittenAsDouble(x) {
    var a = x * 1.0;
    var b = 1. * x;
    if (!(isNaN(x) && isNaN(a) && isNaN(b)) && a !== b)
        throw "Internal error on arithMulIdentityWrittenAsDouble, a = " + a + " b = " + b;
    return a;
}
noInline(arithMulIdentityWrittenAsDouble);

function testArithMulIdentityWrittenAsDouble() {
    for (var i = 0; i < 1e4; ++i) {
        var result = arithMulIdentityWrittenAsDouble(i);
        if (result !== i) {
            throw "arithMulIdentityWrittenAsDouble(i) = " + result + ", expected " + i;
        }
    }

    for (var i = 0; i < 1e4; ++i) {
        var result = arithMulIdentityWrittenAsDouble(-0);
        if (result !== -0) {
            throw "arithMulIdentityWrittenAsDouble(-0) = " + result + ", expected -0 ";
        }
    }

    for (var i = 0; i < 1e4; ++i) {
        var testValue = i + .5;
        var result = arithMulIdentityWrittenAsDouble(testValue);
        if (result !== testValue) {
            throw "arithMulIdentityWrittenAsDouble(i) = " + result + ", expected " + testValue;
        }
    }

    for (var i = 0; i < 1e4; ++i) {;
        var result = arithMulIdentityWrittenAsDouble(NaN);
        if (!isNaN(result)) {
            throw "arithMulIdentityWrittenAsDouble(NaN) = " + result + ", expected NaN";
        }
    }

    for (var i = 0; i < 1e4; ++i) {;
        var result = arithMulIdentityWrittenAsDouble(Infinity);
        if (isFinite(result)) {
            throw "arithMulIdentityWrittenAsDouble(Infinity) = " + result + ", expected Infinity";
        }
    }

    for (var i = 0; i < 1e4; ++i) {;
        var result = arithMulIdentityWrittenAsDouble(-Infinity);
        if (isFinite(result) || result >= 0) {
            throw "arithMulIdentityWrittenAsDouble(-Infinity) = " + result + ", expected -Infinity";
        }
    }
}
testArithMulIdentityWrittenAsDouble();


// Test "value * 42".
function arithMul42WrittenAsInteger(x) {
    var a = x * 42;
    var b = 42 * x;
    if (!(isNaN(x) && isNaN(a) && isNaN(b)) && a !== b)
        throw "Internal error on arithMul42WrittenAsInteger, a = " + a + " b = " + b;
    return a;
}
noInline(arithMul42WrittenAsInteger);

function testArithMul42WrittenAsInteger() {
    for (var i = 0; i < 1e4; ++i) {
        var result = arithMul42WrittenAsInteger(13);
        if (result !== 546) {
            throw "arithMul42WrittenAsInteger(13) = " + result + ", expected 546";
        }
    }

    for (var i = 0; i < 1e4; ++i) {
        var result = arithMul42WrittenAsInteger(-0);
        if (result !== -0) {
            throw "arithMul42WrittenAsInteger(-0) = " + result + ", expected -0";
        }
    }

    for (var i = 0; i < 1e4; ++i) {
        var result = arithMul42WrittenAsInteger(13.3);
        if (result !== 558.6) {
            throw "arithMul42WrittenAsInteger(13.3) = " + result + ", expected 558.6";
        }
    }

    for (var i = 0; i < 1e4; ++i) {;
        var result = arithMul42WrittenAsInteger(NaN);
        if (!isNaN(result)) {
            throw "arithMul42WrittenAsInteger(NaN) = " + result + ", expected NaN";
        }
    }

    for (var i = 0; i < 1e4; ++i) {;
        var result = arithMul42WrittenAsInteger(Infinity);
        if (isFinite(result)) {
            throw "arithMul42WrittenAsInteger(Infinity) = " + result + ", expected Infinity";
        }
    }

    for (var i = 0; i < 1e4; ++i) {;
        var result = arithMul42WrittenAsInteger(-Infinity);
        if (isFinite(result) || result >= 0) {
            throw "arithMul42WrittenAsInteger(-Infinity) = " + result + ", expected -Infinity";
        }
    }
}
testArithMul42WrittenAsInteger();


function arithMul42WrittenAsDouble(x) {
    var a = x * 42.0;
    var b = 42. * x;
    if (!(isNaN(x) && isNaN(a) && isNaN(b)) && a !== b)
        throw "Internal error on arithMul42WrittenAsDouble, a = " + a + " b = " + b;
    return a;
}
noInline(arithMul42WrittenAsDouble);

function testArithMul42WrittenAsDouble() {
    for (var i = 0; i < 1e4; ++i) {
        var result = arithMul42WrittenAsDouble(13);
        if (result !== 546) {
            throw "arithMul42WrittenAsDouble(i) = " + result + ", expected 546";
        }
    }

    for (var i = 0; i < 1e4; ++i) {
        var result = arithMul42WrittenAsDouble(-0);
        if (result !== -0) {
            throw "arithMul42WrittenAsDouble(-0) = " + result + ", expected -0";
        }
    }

    for (var i = 0; i < 1e4; ++i) {
        var result = arithMul42WrittenAsDouble(13.3);
        if (result !== 558.6) {
            throw "arithMul42WrittenAsDouble(13.3) = " + result + ", expected 558.6";
        }
    }

    for (var i = 0; i < 1e4; ++i) {;
        var result = arithMul42WrittenAsDouble(NaN);
        if (!isNaN(result)) {
            throw "arithMul42WrittenAsDouble(NaN) = " + result + ", expected NaN";
        }
    }

    for (var i = 0; i < 1e4; ++i) {;
        var result = arithMul42WrittenAsDouble(Infinity);
        if (isFinite(result)) {
            throw "arithMul42WrittenAsDouble(Infinity) = " + result + ", expected Infinity";
        }
    }

    for (var i = 0; i < 1e4; ++i) {;
        var result = arithMul42WrittenAsDouble(-Infinity);
        if (isFinite(result) || result >= 0) {
            throw "arithMul42WrittenAsDouble(-Infinity) = " + result + ", expected -Infinity";
        }
    }
}
testArithMul42WrittenAsDouble();

function testArithMulWithTypeConfusedConstant() {
    let v1 = 1.0;

    function testMult(v2) {
        let v3 = [];
        if (v3) {
            v3 = v1 + 1;
        }
        return v2 * v3;
    }

    for (let i = 13.37; i < 10000; i++) {
        let result = testMult(i);
        if ((result / 2 - i) > 0.1E-20)
            throw "testArithMulWithTypeConfusedConstant(i) = " + result + ", expected " + (i * 2);
    }
}
testArithMulWithTypeConfusedConstant();
