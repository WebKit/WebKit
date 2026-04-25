function assert(b) {
    if (!b)
        throw new Error;
}

let intNumber = 10
let doubleNumber = 1.2345;

// ------------------------ Constant Character and Number ------------------------
function lessConstantChar(x) { return x < 'a'; }
function lessEqConstantChar(x) { return x <= 'a'; }
function greaterConstantChar(x) { return x > 'a'; }
function greaterEqConstantChar(x) { return x >= 'a'; }

function numberToConstantChar(x, expected) {
    let b1 = lessConstantChar(x);
    assert(b1 == expected[0]);
    let b2 = lessEqConstantChar(x);
    assert(b2 == expected[1]);
    let b3 = greaterConstantChar(x);
    assert(b3 == expected[2]);
    let b4 = greaterEqConstantChar(x);
    assert(b4 == expected[3]);
    return b1 | b2 | b3 | b4;
}

function constantCharLess(x) { return 'a' < x; }
function constantCharLessEq(x) { return 'a' <= x; }
function constantChaGreater(x) { return 'a' > x; }
function constantCharGreaterEq(x) { return 'a' >= x; }

function constantCharToNumber(x, expected) {
    let b1 = constantCharLess(x);
    assert(b1 == expected[0]);
    let b2 = constantCharLessEq(x);
    assert(b2 == expected[1]);
    let b3 = constantChaGreater(x);
    assert(b3 == expected[2]);
    let b4 = constantCharGreaterEq(x);
    assert(b4 == expected[3]);
    return b1 | b2 | b3 | b4;
}

let expectedIntToConstantChar = [
    lessConstantChar(intNumber),
    lessEqConstantChar(intNumber),
    greaterConstantChar(intNumber),
    greaterEqConstantChar(intNumber)
];

function intToConstantChar(x) { return numberToConstantChar(x, expectedIntToConstantChar); }

let expectedConstantCharToInt = [
    constantCharLess(intNumber),
    constantCharLessEq(intNumber),
    constantChaGreater(intNumber),
    constantCharGreaterEq(intNumber)
];

function constantCharToInt(x) { return constantCharToNumber(x, expectedConstantCharToInt); }

let expectedDoubleToConstantChar = [
    lessConstantChar(doubleNumber),
    lessEqConstantChar(doubleNumber),
    greaterConstantChar(doubleNumber),
    greaterEqConstantChar(doubleNumber)
];

function doubleToConstantChar(x) { return numberToConstantChar(x, expectedDoubleToConstantChar); }

let expectedConstantCharToDouble = [
    constantCharLess(doubleNumber),
    constantCharLessEq(doubleNumber),
    constantChaGreater(doubleNumber),
    constantCharGreaterEq(doubleNumber)
];

function constantCharToDouble(x) { return constantCharToNumber(x, expectedConstantCharToDouble); }

// ------------------------ Imm and Number ------------------------
function lessImm(x) { return x < 10; }
function lessEqImm(x) { return x <= 10; }
function greaterImm(x) { return x > 10; }
function greaterEqImm(x) { return x >= 10; }

function numberToImm(x, expected) {
    let b1 = lessImm(x);
    assert(b1 == expected[0]);
    let b2 = lessEqImm(x);
    assert(b2 == expected[1]);
    let b3 = greaterImm(x);
    assert(b3 == expected[2]);
    let b4 = greaterEqImm(x);
    assert(b4 == expected[3]);
    return b1 | b2 | b3 | b4;
}

function immLess(x) { return 10 < x; }
function immLessEq(x) { return 10 <= x; }
function immGreater(x) { return 10 > x; }
function immGreaterEq(x) { return 10 >= x; }

function immToNumber(x, expected) {
    let b1 = immLess(x);
    assert(b1 == expected[0]);
    let b2 = immLessEq(x);
    assert(b2 == expected[1]);
    let b3 = immGreater(x);
    assert(b3 == expected[2]);
    let b4 = immGreaterEq(x);
    assert(b4 == expected[3]);
    return b1 | b2 | b3 | b4;
}

let expectedIntToImm = [
    lessImm(intNumber),
    lessEqImm(intNumber),
    greaterImm(intNumber),
    greaterEqImm(intNumber)
];

function intToImm(x) { return numberToImm(x, expectedIntToImm); }

let expectedImmToInt = [
    immLess(intNumber),
    immLessEq(intNumber),
    immGreater(intNumber),
    immGreaterEq(intNumber)
];

function immToInt(x) { return immToNumber(x, expectedImmToInt); }

let expectedDoubleToImm = [
    lessImm(doubleNumber),
    lessEqImm(doubleNumber),
    greaterImm(doubleNumber),
    greaterEqImm(doubleNumber)
];

function doubleToImm(x) { return numberToImm(x, expectedDoubleToImm); }

let expectedImmToDouble = [
    immLess(doubleNumber),
    immLessEq(doubleNumber),
    immGreater(doubleNumber),
    immGreaterEq(doubleNumber)
];

function immToDouble(x) { return immToNumber(x, expectedImmToDouble); }

// ------------------------ Number and Number ------------------------

function numLessNum(x, y) { return x < y; }
function numLessEqNum(x, y) { return x <= y; }
function numGreaterNum(x, y) { return x > y; }
function numGreaterEqNum(x, y) { return x >= y; }

function numberToNumber(x, y, expected) {
    let b1 = numLessNum(x, y);
    assert(b1 == expected[0]);
    let b2 = numLessEqNum(x, y);
    assert(b2 == expected[1]);
    let b3 = numGreaterNum(x, y);
    assert(b3 == expected[2]);
    let b4 = numGreaterEqNum(x, y);
    assert(b4 == expected[3]);
    return b1 | b2 | b3 | b4;
}

let expectedIntToInt = [
    numLessNum(intNumber, intNumber),
    numLessEqNum(intNumber, intNumber),
    numGreaterNum(intNumber, intNumber),
    numGreaterEqNum(intNumber, intNumber)
];

function intToInt(x, y) { return numberToNumber(x, y, expectedIntToInt); }

let expectedIntToDouble = [
    numLessNum(intNumber, doubleNumber),
    numLessEqNum(intNumber, doubleNumber),
    numGreaterNum(intNumber, doubleNumber),
    numGreaterEqNum(intNumber, doubleNumber)
];

function intToDouble(x, y) { return numberToNumber(x, y, expectedIntToDouble); }

let expectedDoubleToInt = [
    numLessNum(doubleNumber, intNumber),
    numLessEqNum(doubleNumber, intNumber),
    numGreaterNum(doubleNumber, intNumber),
    numGreaterEqNum(doubleNumber, intNumber)
];

function doubleToInt(x, y) { return numberToNumber(x, y, expectedDoubleToInt); }

let expectedDoubleToDouble = [
    numLessNum(doubleNumber, doubleNumber),
    numLessEqNum(doubleNumber, doubleNumber),
    numGreaterNum(doubleNumber, doubleNumber),
    numGreaterEqNum(doubleNumber, doubleNumber)
];

function doubleToDouble(x, y) { return numberToNumber(x, y, expectedDoubleToDouble); }

// ------------------------ Main ------------------------

let res = false;
for (let i = 0; i < 100; i++) {
    res |= intToConstantChar(intNumber);
    res |= constantCharToInt(intNumber);
    res |= doubleToConstantChar(doubleNumber);
    res |= constantCharToDouble(doubleNumber);

    res |= intToImm(intNumber);
    res |= immToInt(intNumber);
    res |= doubleToImm(doubleNumber);
    res |= immToDouble(doubleNumber);

    res |= intToInt(intNumber, intNumber);
    res |= intToDouble(intNumber, doubleNumber);
    res |= doubleToInt(doubleNumber, intNumber);
    res |= doubleToDouble(doubleNumber, doubleNumber);
}

