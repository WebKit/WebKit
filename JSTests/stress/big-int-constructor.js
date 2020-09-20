function assert(a) {
    if (!a)
        throw new Error("Bad assertion");
}

function assertThrowSyntaxError(input) {
    try {
        let n = BigInt(input);
        assert(false);
    } catch (e) {
        assert(e instanceof SyntaxError);
    }
}

function assertThrowRangeError(input) {
    try {
        let n = BigInt(input);
        assert(false);
    } catch (e) {
        assert(e instanceof RangeError);
    }
}

function assertThrowTypeError(input) {
    try {
        let n = BigInt(input);
        assert(false);
    } catch (e) {
        assert(e instanceof TypeError);
    }
}

// Test 0 conversions
let n = BigInt("");
assert(n.toString() === "0");

n = BigInt("  ");
assert(n.toString() === "0");

n = BigInt("0");
assert(n.toString() === "0");

n = BigInt("+0");
assert(n.toString() === "0");

n = BigInt("-0");
assert(n.toString() === "0");

n = BigInt("  0");
assert(n.toString() === "0");

n = BigInt("0  ");
assert(n.toString() === "0");

n = BigInt("  0  ");
assert(n.toString() === "0");

n = BigInt("00000");
assert(n.toString() === "0");

let giantTrailingString = "0";
for (let i = 0; i < 10000; i++)
    giantTrailingString += " ";

n = BigInt(giantTrailingString);
assert(n.toString() === "0");

// Binary representation

n = BigInt("0b1111");
assert(n.toString() === "15");

n = BigInt("0b10");
assert(n.toString() === "2");

n = BigInt("0b10");
assert(n.toString() === "2");

let binaryString = "0b1";
for (let i = 0; i < 128; i++)
    binaryString += "0";

n = BigInt(binaryString);
assert(n.toString() === "340282366920938463463374607431768211456");

n = BigInt("0B1111");
assert(n.toString() === "15");

n = BigInt("0B10");
assert(n.toString() === "2");

n = BigInt("0B10");
assert(n.toString() === "2");

binaryString = "0B1";
for (let i = 0; i < 128; i++)
    binaryString += "0";

n = BigInt(binaryString);
assert(n.toString() === "340282366920938463463374607431768211456");

// Octal representation
 
n = BigInt("0o7");
assert(n.toString() === "7");

n = BigInt("0o10");
assert(n.toString() === "8");

n = BigInt("0o20");
assert(n.toString() === "16");

n = BigInt("   0o20");
assert(n.toString() === "16");

n = BigInt("   0o20  ");
assert(n.toString() === "16");

n = BigInt("0O7");
assert(n.toString() === "7");

n = BigInt("0O10");
assert(n.toString() === "8");

n = BigInt("0O20");
assert(n.toString() === "16");

n = BigInt("   0O20");
assert(n.toString() === "16");

n = BigInt("   0O20  ");
assert(n.toString() === "16");

// Hexadecimal representation

n = BigInt("0xa");
assert(n.toString() === "10");

n = BigInt("0xff");
assert(n.toString() === "255");

n = BigInt("  0xff  ");
assert(n.toString() === "255");

n = BigInt("  0xfabc  ");
assert(n.toString() === "64188");

// Number conversion

n = BigInt(3245);
assert(n.toString() === "3245");

n = BigInt(-2147483648)
assert(n.toString() === "-2147483648");

n = BigInt(0);
assert(n.toString() === "0");

n = BigInt(-46781);
assert(n.toString() === "-46781");

// Int53
n = BigInt(4503599627370490);
assert(n.toString() === "4503599627370490");

n = BigInt(-4503599627370490);
assert(n.toString() === "-4503599627370490");

n = BigInt(-4503599627370496);
assert(n.toString() === "-4503599627370496");

// Boolean conversion
n = BigInt(true);
assert(n.toString() === "1");

n = BigInt(false);
assert(n.toString() === "0");

// Objects
let o = {
    valueOf: function () {
        return 3;
    }
}

n = BigInt(o);
assert(n.toString() === "3");

o = {
    valueOf: function () {
        return "54";
    }
}

n = BigInt(o);
assert(n.toString() === "54");

o = {
    toString: function () {
        return "5489543";
    }
}

n = BigInt(o);
assert(n.toString() === "5489543");

o = {
    toString: function () {
        return 135489543;
    }
}

n = BigInt(o);
assert(n.toString() === "135489543");

o = {
    valueOf: function () {
        return 3256;
    },

    toString: function () {
        return "563737";
    }
}

n = BigInt(o);
assert(n.toString() === "3256");

// Assertion thows

assertThrowSyntaxError("aba");
assertThrowSyntaxError("-0x1");
assertThrowSyntaxError("-0XFFab");
assertThrowSyntaxError("0o78");
assertThrowSyntaxError("0oa");
assertThrowSyntaxError("000 12");
assertThrowSyntaxError("0o");
assertThrowSyntaxError("0b");
assertThrowSyntaxError("0x");
assertThrowSyntaxError("00o");
assertThrowSyntaxError("00b");
assertThrowSyntaxError("00x");
assertThrowTypeError(null);
assertThrowTypeError(undefined);
assertThrowTypeError(Symbol("a"));
assertThrowRangeError(0.5);
assertThrowRangeError(-.5);
assertThrowRangeError(Infinity);
assertThrowRangeError(-Infinity);
assertThrowRangeError(NaN);

// Object throwing error

o = {
    valueOf: function () {
        throw new Error("MyError");
    }
}

try {
    n = BigInt(o);
    assert(false);
} catch(e) {
    assert(e.message === "MyError");
}

try {
    n = BigInt();
    assert(false);
} catch(e) {
    assert(e instanceof TypeError);
}

