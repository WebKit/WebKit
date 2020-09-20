function assert(a, e, m) {
    if (a !== e)
        throw new Error(m);
}

function testEquals(a, b, e) {
    assert(a == b, e, a + " == " + b + " should be " + e);
    assert(b == a, e, b + " == " + a + " should be " + e);
}

function testEqualsWithMessage(a, b, e, m) {
    assert(a == b, e, m);
    assert(b == a, e, m);
}

// BigInt - BigInt
testEquals(1n, 1n, true);
testEquals(1928392129312n, 1n, false);
testEquals(0n, 1n, false);
testEquals(0n, 0n, true);
testEquals(817283912931n, 817283912931n, true);
testEquals(0xFFD817283AF9129E31n, 0xFFD817283AF9129E31n, true);
testEquals(0xAFFD817283AF9129E31n, 0xFFD817283AF9129E31n, false);
testEquals(4719490697266344402481n, BigInt("-4719490697266344402481"), false);
testEquals(BigInt("-4719490697266344402481"), BigInt("4719490697266344402481"), false);
testEquals(BigInt("-4719490697266344402481"), BigInt("-4719490697266344402481"), true);
testEquals(BigInt("-17"), BigInt("-17"), true);

// BigInt - String

testEquals(1n, "1", true);
testEquals(1928392129312n, "1", false);
testEquals(0n, "1", false);
testEquals(0n, "0", true);
testEquals(817283912931n, "817283912931", true);
testEquals(0xFFD817283AF9129E31n, "4719490697266344402481", true);
testEquals(0xAFFD817283AF9129E31n, "4719490697266344402481", false);

// BigInt - Number

testEquals(0n, 0, true);
testEquals(0n, -0, true);
testEquals(-0, 0n, true);
testEquals(0n, 0.000000000001, false);
testEquals(0n, 1, false);
testEquals(1, 0n, false);
testEquals(1n, 0.999999999999, false);
testEquals(1n, 1, true);
testEquals(0n, Number.MIN_VALUE, false);
testEquals(0n, -Number.MIN_VALUE, false);
testEquals(BigInt("-10"), Number.MIN_VALUE, false);
testEquals(1n, Number.MAX_VALUE, false);
testEquals(1n, -Number.MAX_VALUE, false);
testEquals(0xfffffffffffff7ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffn, Number.MAX_VALUE, false);
testEquals(0xfffffffffffff800000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000n, Number.MAX_VALUE, true);
testEquals(0xfffffffffffff800000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001n, Number.MAX_VALUE, false);
testEquals(230000000000000000000, 230000000000000000000n, true);
testEquals(10n, NaN, false);
testEquals(10n, undefined, false);
testEquals(10n, null, false);
testEquals(10n, Infinity, false);
testEquals(10n, -Infinity, false);
testEquals(BigInt("-2147483648"), -2147483648, true); // Testing INT32_MIN
testEquals(BigInt("-2147483647"), -2147483648, false);
testEquals(BigInt("2147483647"), -2147483648, false);
testEquals(BigInt("2147483648"), -2147483648, false);
testEquals(BigInt("2147483647"), 2147483647, true);
testEquals(BigInt("2147483648"), 2147483647, false);

// BigInt - Boolean

testEquals(BigInt("-1"), false, false);
testEquals(BigInt("-1"), true, false);
testEquals(0n, false, true);
testEquals(0n, true, false);
testEquals(1n, false, false);
testEquals(1n, true, true);
testEquals(2n, false, false);
testEquals(2n, true, false);

// BigInt - Object

testEquals(0n, Object(0n), true);
testEquals(0n, Object(1n), false);
testEquals(1n, Object(0n), false);
testEquals(1n, Object(1n), true);
testEquals(2n, Object(0n), false);
testEquals(2n, Object(1n), false);
testEquals(2n, Object(2n), true);
testEquals(0n, {}, false);
testEquals(0n, {valueOf: function() { return 0n; }}, true);
testEquals(0n, {valueOf: function() { return 1n; }}, false);
testEquals(0n, {toString: function() { return "0"; }}, true);
testEquals(0n, {toString: function() { return "1"; }}, false);
testEquals(900719925474099101n, {valueOf: function() { return 900719925474099101n; }}, true);
testEquals(900719925474099101n, {valueOf: function() { return 900719925474099102n; }}, false);
testEquals(900719925474099101n, {toString: function() { return "900719925474099101"; }}, true);
testEquals(900719925474099101n, {toString: function() { return "900719925474099102"; }}, false);

try {
    let o = {valueOf: function() { throw new Error("my error"); }};
    o == 1n;
    throw new Error("Exception in ToPrimitive not catched");
} catch(e) {
    assert(e.message, "my error", "Wrong exception in ToPrimitive");
}

try {
    let o = {toString: function() { throw new Error("my error"); }};
    o == 1n;
    throw new Error("Exception in ToString not catched");
} catch(e) {
    assert(e.message, "my error", "Wrong exception in ToString");
}

// BigInt - Symbol

testEqualsWithMessage(0n, Symbol("1"), false, "0n == Symbol(1)");
testEqualsWithMessage(Symbol("1"), 0n, false, "Symbol(1) == 0n");
testEqualsWithMessage(1n, Symbol("1"), false, "1n == Symbol(1)");
testEqualsWithMessage(Symbol("1"), 1n, false, "Symbol(1) == 1n");

