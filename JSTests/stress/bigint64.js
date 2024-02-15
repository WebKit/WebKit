//@ requireOptions("--useDollarVM=true")

function format(n) {
    return n + ":" + typeof n;
}

function shouldThrow(func, message) {
    var error = null;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!error)
        throw new Error("not thrown.");
    if (String(error) !== message)
        throw new Error("bad error: " + String(error));
}

function check(actual, expected) {
    actual = format(actual);
    expected = format(expected);
    if (actual !== expected)
        throw new Error("bad! actual was " + actual + " but expect " + expected);
}

function test(expected, op, a, b) {
    check(op(a, b), expected);
}

function createLoopFunc(op) {
    return function (a, b) {
        let res = 0n;
        for (let i = 0; i < 10; i++)
            res += op(a, b);
        return res;
    }
}

function dfg(func) {
    if ($vm.dfgTrue()) {
        func();
    }
}

function ftl(func) {
    if ($vm.ftlTrue()) {
        func();
    }
}

// ------------------------------------------------------------

function neg(a) { return -a; }
function inc(a) { return ++a; }
function dec(a) { return --a; }
function bitNot(a) { return ~a; }

function bitAnd(a, b) { return a & b; }
function bitOr(a, b) { return a | b; }
function bitXor(a, b) { return a ^ b; }
function bitRShift(a, b) { return a >> b; }
function bitLShift(a, b) { return a << b; }

function sub(a, b) { return a - b; }
function add(a, b) { return a + b; }
function mul(a, b) { return a * b; }
function div(a, b) { return a / b; }
function mod(a, b) { return a % b; }

function pow(a, b) { return a ** b; }

function eq(a, b) { return a == b; }
function strictEq(a, b) { return a === b; }
function less(a, b) { return a < b; }
function lessEq(a, b) { return a <= b; }
function greater(a, b) { return a > b; }
function greaterEq(a, b) { return a >= b; }

function addMul(a, b) { return (a + b) * b; }
function negMul(a, b) { return (-a) * b; }
function incMul(a, b) { return (++a) * b; }
function decMul(a, b) { return (--a) * b; }

// ------------------------------------------------------------

let BIGINT64_MAX = 0x7FFF_FFFF_FFFF_FFFFn;
let BIGINT64_MAX_MINUS_ONE = 0x7FFF_FFFF_FFFF_FFFEn;
let BIGINT64_MAX_PLUS_ONE = 0x8000_0000_0000_0000n;
let BIGINT64_MAX_PLUS_TWO = 0x8000_0000_0000_0001n;

let BIGINT64_MIN = -0x8000_0000_0000_0000n;
let BIGINT64_MIN_PLUS_ONE = -0x7FFF_FFFF_FFFF_FFFFn;
let BIGINT64_MIN_MINUS_ONE = -0x8000_0000_0000_0001n;
let BIGINT64_MIN_MINUS_TWO = -0x8000_0000_0000_0002n;

for (var i = 0; i < 1e4; ++i) {
    // print("iteration ", i);

    // -------- int64 zero operands no overflow -------- 
    test(0n, add, 0n, 0n);
    test(0n, sub, 0n, 0n);
    test(1n, inc, 0n);
    test(-1n, dec, 0n);
    test(-1n, bitNot, 0n);
    test(0n, bitAnd, 0n, 0n);
    test(0n, bitOr, 0n, 0n);
    test(0n, bitXor, 0n, 0n);
    test(0n, bitRShift, 0n, 0n);
    test(0n, bitLShift, 0n, 0n);
    test(0n, mul, 0n, 0n);
    test(0n, div, 0n, 1n);
    test(0n, mod, 0n, 1n);
    test(true, eq, 0n, 0n);
    test(true, strictEq, 0n, 0n);
    test(false, less, 0n, 0n);
    test(true, lessEq, 0n, 0n);
    test(false, greater, 0n, 0n);
    test(true, greaterEq, 0n, 0n);
    test(1n, pow, 0n, 0n);

    // -------- int64 positive operands no overflow -------- 
    test(BIGINT64_MAX, add, BIGINT64_MAX_MINUS_ONE, 1n);
    test(BIGINT64_MAX, add, BIGINT64_MAX, 0n);
    test(BIGINT64_MIN_PLUS_ONE, neg, BIGINT64_MAX);
    test(BIGINT64_MAX, inc, BIGINT64_MAX_MINUS_ONE);
    test(BIGINT64_MAX_MINUS_ONE, dec, BIGINT64_MAX);
    test(BIGINT64_MIN, bitNot, BIGINT64_MAX);
    test(0n, bitAnd, 0b1010n, 0b0101n);
    test(0b1111n, bitOr, 0b1010n, 0b0101n);
    test(0n, bitXor, BIGINT64_MAX, BIGINT64_MAX);
    test(BIGINT64_MAX_MINUS_ONE / 2n, bitRShift, BIGINT64_MAX_MINUS_ONE, 1n);
    test(BIGINT64_MAX_MINUS_ONE, bitLShift, BIGINT64_MAX_MINUS_ONE / 2n, 1n);
    test(0n, mul, BIGINT64_MAX, 0n);
    test(BIGINT64_MAX, div, BIGINT64_MAX, 1n);
    test(0n, mod, BIGINT64_MAX, 1n);
    test(true, eq, BIGINT64_MAX, BIGINT64_MAX);
    test(true, strictEq, BIGINT64_MAX, BIGINT64_MAX);
    test(false, less, BIGINT64_MAX, BIGINT64_MAX);
    test(true, lessEq, BIGINT64_MAX, BIGINT64_MAX);
    test(false, greater, BIGINT64_MAX, BIGINT64_MAX);
    test(true, greaterEq, BIGINT64_MAX, BIGINT64_MAX);
    test(16n, pow, 4n, 2n);
    test(16n, pow, 2n, 4n);

    // -------- int64 negative operands no overflow -------- 
    test(BIGINT64_MIN_PLUS_ONE, add, BIGINT64_MIN, 1n);
    test(BIGINT64_MIN, sub, BIGINT64_MIN_PLUS_ONE, 1n);
    test(BIGINT64_MAX, neg, BIGINT64_MIN_PLUS_ONE);
    test(BIGINT64_MIN, dec, BIGINT64_MIN_PLUS_ONE);
    test(BIGINT64_MAX, bitNot, BIGINT64_MIN);
    test(BIGINT64_MIN, bitAnd, BIGINT64_MIN, BIGINT64_MIN);
    test(BIGINT64_MIN, bitOr, BIGINT64_MIN, BIGINT64_MIN);
    test(0n, bitXor, BIGINT64_MIN, BIGINT64_MIN);
    test(BIGINT64_MIN / 2n, bitRShift, BIGINT64_MIN, 1n);
    test(BIGINT64_MIN, bitLShift, BIGINT64_MIN / 2n, 1n);
    test(0n, mul, BIGINT64_MIN, 0n);
    test(BIGINT64_MIN, div, BIGINT64_MIN, 1n);
    test(0n, mod, BIGINT64_MIN, 1n);
    test(true, eq, BIGINT64_MIN, BIGINT64_MIN);
    test(true, strictEq, BIGINT64_MIN, BIGINT64_MIN);
    test(false, less, BIGINT64_MIN, BIGINT64_MIN);
    test(true, lessEq, BIGINT64_MIN, BIGINT64_MIN);
    test(false, greater, BIGINT64_MIN, BIGINT64_MIN);
    test(true, greaterEq, BIGINT64_MIN, BIGINT64_MIN);
    test(16n, pow, -4n, 2n);
    test(16n, pow, -2n, 4n);
    test(-64n, pow, -4n, 3n);

    // -------- int64 positive operand overflow -------- 
    test(BIGINT64_MAX_PLUS_TWO, add, BIGINT64_MAX_PLUS_ONE, 1n);
    test(BIGINT64_MAX, sub, BIGINT64_MAX_PLUS_ONE, 1n);
    test(BIGINT64_MIN, neg, BIGINT64_MAX_PLUS_ONE);
    test(BIGINT64_MAX_PLUS_TWO, inc, BIGINT64_MAX_PLUS_ONE);
    test(BIGINT64_MAX, dec, BIGINT64_MAX_PLUS_ONE);
    test(BIGINT64_MIN_MINUS_ONE, bitNot, BIGINT64_MAX_PLUS_ONE);
    test(BIGINT64_MAX_PLUS_ONE, bitAnd, BIGINT64_MAX_PLUS_ONE, BIGINT64_MAX_PLUS_ONE);
    test(BIGINT64_MAX_PLUS_ONE, bitOr, BIGINT64_MAX_PLUS_ONE, BIGINT64_MAX_PLUS_ONE);
    test(0n, bitXor, BIGINT64_MAX_PLUS_ONE, BIGINT64_MAX_PLUS_ONE);
    test(BIGINT64_MAX_PLUS_ONE / 2n, bitRShift, BIGINT64_MAX_PLUS_ONE, 1n);
    test(BIGINT64_MAX_PLUS_ONE * 2n, bitLShift, BIGINT64_MAX_PLUS_ONE, 1n);
    test(0n, mul, BIGINT64_MAX_PLUS_ONE, 0n);
    test(BIGINT64_MAX_PLUS_ONE, div, BIGINT64_MAX_PLUS_ONE, 1n);
    test(0n, mod, BIGINT64_MAX_PLUS_ONE, 1n);
    test(false, eq, BIGINT64_MAX_PLUS_ONE, BIGINT64_MAX);
    test(false, strictEq, BIGINT64_MAX_PLUS_ONE, BIGINT64_MAX);
    test(false, less, BIGINT64_MAX_PLUS_ONE, BIGINT64_MAX);
    test(false, lessEq, BIGINT64_MAX_PLUS_ONE, BIGINT64_MAX);
    test(true, greater, BIGINT64_MAX_PLUS_ONE, BIGINT64_MAX);
    test(true, greaterEq, BIGINT64_MAX_PLUS_ONE, BIGINT64_MAX);
    test(1n, pow, BIGINT64_MAX_PLUS_ONE, 0n);

    // -------- int64 negative operand overflow -------- 
    test(BIGINT64_MIN, add, BIGINT64_MIN_MINUS_ONE, 1n);
    test(BIGINT64_MIN_MINUS_TWO, sub, BIGINT64_MIN_MINUS_ONE, 1n);
    test(BIGINT64_MAX_PLUS_TWO, neg, BIGINT64_MIN_MINUS_ONE);
    test(BIGINT64_MIN, inc, BIGINT64_MIN_MINUS_ONE);
    test(BIGINT64_MIN_MINUS_TWO, dec, BIGINT64_MIN_MINUS_ONE);
    test(BIGINT64_MAX_PLUS_ONE, bitNot, BIGINT64_MIN_MINUS_ONE);
    test(BIGINT64_MIN_MINUS_ONE, bitAnd, BIGINT64_MIN_MINUS_ONE, BIGINT64_MIN_MINUS_ONE);
    test(BIGINT64_MIN_MINUS_ONE, bitOr, BIGINT64_MIN_MINUS_ONE, BIGINT64_MIN_MINUS_ONE);
    test(0n, bitXor, BIGINT64_MIN_MINUS_ONE, BIGINT64_MIN_MINUS_ONE);
    test(BIGINT64_MIN_MINUS_TWO / 2n, bitRShift, BIGINT64_MIN_MINUS_TWO, 1n);
    test(BIGINT64_MIN_MINUS_TWO * 2n, bitLShift, BIGINT64_MIN_MINUS_TWO, 1n);
    test(0n, mul, BIGINT64_MIN_MINUS_ONE, 0n);
    test(BIGINT64_MIN_MINUS_TWO, div, BIGINT64_MIN_MINUS_TWO, 1n);
    test(0n, mod, BIGINT64_MIN_MINUS_TWO, 1n);
    test(false, eq, BIGINT64_MIN_MINUS_TWO, BIGINT64_MIN);
    test(false, strictEq, BIGINT64_MIN_MINUS_TWO, BIGINT64_MIN);
    test(true, less, BIGINT64_MIN_MINUS_TWO, BIGINT64_MIN);
    test(true, lessEq, BIGINT64_MIN_MINUS_TWO, BIGINT64_MIN);
    test(false, greater, BIGINT64_MIN_MINUS_TWO, BIGINT64_MIN);
    test(false, greaterEq, BIGINT64_MIN_MINUS_TWO, BIGINT64_MIN);
    test(1n, pow, BIGINT64_MIN_MINUS_TWO, 0n);

    // -------- int64 add overflow -------- 
    test(BIGINT64_MAX_PLUS_ONE, add, BIGINT64_MAX, 1n);

    // -------- int64 sub overflow -------- 
    test(BIGINT64_MIN_MINUS_ONE, sub, BIGINT64_MIN, 1n);

    // -------- int64 inc overflow -------- 
    test(BIGINT64_MAX_PLUS_ONE, inc, BIGINT64_MAX);

    // -------- int64 dec overflow -------- 
    test(BIGINT64_MIN_MINUS_ONE, dec, BIGINT64_MIN);

    // -------- int64 right shift overflow -------- 
    test(BIGINT64_MAX * 2n, bitRShift, BIGINT64_MAX, -1n);
    test(BIGINT64_MIN * 2n, bitRShift, BIGINT64_MIN, -1n);

    // -------- int64 left shift overflow -------- 
    test(BIGINT64_MAX * 2n, bitLShift, BIGINT64_MAX, 1n);
    test(BIGINT64_MIN * 2n, bitLShift, BIGINT64_MIN, 1n);

    // -------- int64 mul overflow -------- 
    test(BIGINT64_MIN << 1n, mul, BIGINT64_MIN, 2n);

    // -------- int64 div overflow -------- 
    test(-BIGINT64_MIN, div, BIGINT64_MIN, -1n);

    dfg(() => {
        shouldThrow(() => {
            div(BIGINT64_MIN, 0n);
        }, "RangeError: 0 is an invalid divisor value.");
    });

    ftl(() => {
        shouldThrow(() => {
            div(BIGINT64_MIN, 0n);
        }, "RangeError: 0 is an invalid divisor value.");
    });

    // -------- int64 mod overflow -------- 
    dfg(() => {
        shouldThrow(() => {
            div(BIGINT64_MIN, 0n);
        }, "RangeError: 0 is an invalid divisor value.");
    });

    ftl(() => {
        shouldThrow(() => {
            div(BIGINT64_MIN, 0n);
        }, "RangeError: 0 is an invalid divisor value.");
    });

    // --------- int64 pow overflow ---------
    test(9223372037000250000n, pow, 3037000500n, 2n);

    // -------- int64 op loop -------- 
    test(20n, createLoopFunc(add), 1n, 1n);
    test(-10n, createLoopFunc(sub), 1n, 2n);
    test(-10n, createLoopFunc(neg), 1n);
    test(10n, createLoopFunc(inc), 0n);
    test(-10n, createLoopFunc(dec), 0n);

    // -------- int64 multi-operations -------- 
    test(2n, addMul, 1n, 1n);
    test(-1n, negMul, 1n, 1n);
    test(1n, incMul, 0n, 1n);
    test(-1n, decMul, 0n, 1n);
}

// print("done");
