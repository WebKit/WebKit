// Basic cases of Math.log() when the value passed are constants.

// log(NaN).
function logNaN() {
    return Math.log(NaN);
}
noInline(logNaN);

function testLogNaN() {
    for (var i = 0; i < 10000; ++i) {
        var result = logNaN();
        if (!isNaN(result))
            throw "logNaN() = " + result + ", expected NaN";
    }
}
testLogNaN();


// log(0).
function logZero() {
    return Math.log(0);
}
noInline(logZero);

function testLogZero() {
    for (var i = 0; i < 10000; ++i) {
        var result = logZero();
        if (result !== -Infinity)
            throw "logZero() = " + result + ", expected -Infinity";
    }
}
testLogZero();


// log(1).
function logOne() {
    return Math.log(1);
}
noInline(logOne);

function testLogOne() {
    for (var i = 0; i < 10000; ++i) {
        var result = logOne();
        if (result !== 0)
            throw "logOne(1) = " + result + ", expected 0";
    }
}
testLogOne();


// log(-1).
function logMinusOne() {
    return Math.log(-1);
}
noInline(logMinusOne);

function testLogMinusOne() {
    for (var i = 0; i < 10000; ++i) {
        var result = logMinusOne();
        if (!isNaN(result))
            throw "logMinusOne() = " + result + ", expected NaN";
    }
}
testLogMinusOne();


// log(Infinity).
function logInfinity() {
    return Math.log(Infinity);
}
noInline(logInfinity);

function testLogInfinity() {
    for (var i = 0; i < 10000; ++i) {
        var result = logInfinity();
        if (result !== Infinity)
            throw "logInfinity() = " + result + ", expected Infinity";
    }
}
testLogInfinity();


// log(-Infinity).
function logMinusInfinity() {
    return Math.log(-Infinity);
}
noInline(logMinusInfinity);

function testLogMinusInfinity() {
    for (var i = 0; i < 10000; ++i) {
        var result = logMinusInfinity();
        if (!isNaN(result))
            throw "logMinusInfinity() = " + result + ", expected NaN";
    }
}
testLogMinusInfinity();


// log(integer).
function logInteger() {
    return Math.log(42);
}
noInline(logInteger);

function testLogInteger() {
    for (var i = 0; i < 10000; ++i) {
        var result = logInteger();
        if (result !== 3.7376696182833684)
            throw "logInteger() = " + result + ", expected 3.7376696182833684";
    }
}
testLogInteger();


// log(double).
function logDouble() {
    return Math.log(Math.PI);
}
noInline(logDouble);

function testLogDouble() {
    for (var i = 0; i < 10000; ++i) {
        var result = logDouble();
        if (result !== 1.1447298858494002)
            throw "logDouble() = " + result + ", expected 1.1447298858494002";
    }
}
testLogDouble();