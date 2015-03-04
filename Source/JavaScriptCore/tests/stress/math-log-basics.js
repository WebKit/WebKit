// Basic cases of Math.log().

// log(NaN).
function logNaN(antilogarithm) {
    return Math.log(antilogarithm);
}
noInline(logNaN);

function testLogNaN() {
    for (var i = 0; i < 10000; ++i) {
        var result = logNaN(NaN);
        if (!isNaN(result))
            throw "logNaN(NaN) = " + result + ", expected NaN";
    }
}
testLogNaN();


// log(0).
function logZero(antilogarithm) {
    return Math.log(antilogarithm);
}
noInline(logZero);

function testLogZero() {
    for (var i = 0; i < 10000; ++i) {
        var result = logZero(0);
        if (result !== -Infinity)
            throw "logZero(0) = " + result + ", expected -Infinity";
    }
}
testLogZero();


// log(1).
function logOne(antilogarithm) {
    return Math.log(antilogarithm);
}
noInline(logOne);

function testLogOne() {
    for (var i = 0; i < 10000; ++i) {
        var result = logOne(1);
        if (result !== 0)
            throw "logOne(1) = " + result + ", expected 0";
    }
}
testLogOne();


// log(-1).
function logMinusOne(antilogarithm) {
    return Math.log(antilogarithm);
}
noInline(logMinusOne);

function testLogMinusOne() {
    for (var i = 0; i < 10000; ++i) {
        var result = logMinusOne(-1);
        if (!isNaN(result))
            throw "logMinusOne(-1) = " + result + ", expected NaN";
    }
}
testLogMinusOne();


// log(Infinity).
function logInfinity(antilogarithm) {
    return Math.log(antilogarithm);
}
noInline(logInfinity);

function testLogInfinity() {
    for (var i = 0; i < 10000; ++i) {
        var result = logInfinity(Infinity);
        if (result !== Infinity)
            throw "logInfinity(Infinity) = " + result + ", expected Infinity";
    }
}
testLogInfinity();


// log(-Infinity).
function logMinusInfinity(antilogarithm) {
    return Math.log(antilogarithm);
}
noInline(logMinusInfinity);

function testLogMinusInfinity() {
    for (var i = 0; i < 10000; ++i) {
        var result = logMinusInfinity(-Infinity);
        if (!isNaN(result))
            throw "logMinusInfinity(-Infinity) = " + result + ", expected NaN";
    }
}
testLogMinusInfinity();


// log(integer).
function logInteger(antilogarithm) {
    return Math.log(antilogarithm);
}
noInline(logInteger);

function testLogInteger() {
    for (var i = 0; i < 10000; ++i) {
        var result = logInteger(42);
        if (result !== 3.7376696182833684)
            throw "logInteger(42) = " + result + ", expected 3.7376696182833684";
    }
}
testLogInteger();


// log(double).
function logDouble(antilogarithm) {
    return Math.log(antilogarithm);
}
noInline(logDouble);

function testLogDouble() {
    for (var i = 0; i < 10000; ++i) {
        var result = logDouble(Math.PI);
        if (result !== 1.1447298858494002)
            throw "logDouble(Math.PI) = " + result + ", expected 1.1447298858494002";
    }
}
testLogDouble();