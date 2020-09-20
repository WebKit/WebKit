// Copyright (C) 2017 Robin Templeton. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

function assert(a) {
    if (!a)
        throw new Error("Bad assertion");
}

assert.sameValue = function (input, expected, message) {
    if (input !== expected)
        throw new Error(message);
}

function testDiv(x, y, z) {
    assert.sameValue(x / y, z, x + " / " + y + " = " + (x/y) + " but should be " + z);
}

testDiv(0xFEDCBA9876543210n, 0xFEDCBA9876543210n, 0x1n);
testDiv(0xFEDCBA9876543210n, 0xFEDCBA987654320Fn, 0x1n);
testDiv(0xFEDCBA9876543210n, 0xFEDCBA98n, 0x100000000n);
testDiv(0xFEDCBA9876543210n, 0xFEDCBA97n, 0x100000001n);
testDiv(0xFEDCBA9876543210n, 0x1234n, 0xE0042813BE5DCn);
testDiv(0xFEDCBA9876543210n, 0x3n, 0x54F43E32D21C10B0n);
testDiv(0xFEDCBA9876543210n, 0x2n, 0x7F6E5D4C3B2A1908n);
testDiv(0xFEDCBA9876543210n, 0x1n, 0xFEDCBA9876543210n);
testDiv(0xFEDCBA9876543210n, BigInt("-1"), BigInt("-18364758544493064720"));
testDiv(0xFEDCBA9876543210n, BigInt("-2"), BigInt("-9182379272246532360"));
testDiv(0xFEDCBA9876543210n, BigInt("-3"), BigInt("-6121586181497688240"));
testDiv(0xFEDCBA9876543210n, BigInt("-4275878551"), BigInt("-4294967297"));
testDiv(0xFEDCBA9876543210n, BigInt("-18364758544493064719"), BigInt("-1"));
testDiv(0xFEDCBA987654320Fn, 0xFEDCBA9876543210n, 0x0n);
testDiv(0xFEDCBA987654320Fn, 0xFEDCBA987654320Fn, 0x1n);
testDiv(0xFEDCBA987654320Fn, 0xFEDCBA98n, 0x100000000n);
testDiv(0xFEDCBA987654320Fn, 0xFEDCBA97n, 0x100000001n);
testDiv(0xFEDCBA987654320Fn, 0x1234n, 0xE0042813BE5DCn);
testDiv(0xFEDCBA987654320Fn, 0x3n, 0x54F43E32D21C10AFn);
testDiv(0xFEDCBA987654320Fn, 0x2n, 0x7F6E5D4C3B2A1907n);
testDiv(0xFEDCBA987654320Fn, 0x1n, 0xFEDCBA987654320Fn);
testDiv(0xFEDCBA98n, 0xFEDCBA9876543210n, 0x0n);
testDiv(0xFEDCBA98n, 0xFEDCBA987654320Fn, 0x0n);
testDiv(0xFEDCBA98n, 0xFEDCBA98n, 0x1n);
testDiv(0xFEDCBA98n, 0xFEDCBA97n, 0x1n);
testDiv(0xFEDCBA98n, 0x1234n, 0xE0042n);
testDiv(0xFEDCBA98n, 0x3n, 0x54F43E32n);
testDiv(0xFEDCBA98n, 0x2n, 0x7F6E5D4Cn);
testDiv(0xFEDCBA98n, 0x1n, 0xFEDCBA98n);
testDiv(0xFEDCBA98n, BigInt("-1"), BigInt("-4275878552"));
testDiv(0xFEDCBA98n, BigInt("-2"), BigInt("-2137939276"));
testDiv(0xFEDCBA98n, BigInt("-3"), BigInt("-1425292850"));
testDiv(0xFEDCBA98n, BigInt("-4275878551"), BigInt("-1"));
testDiv(0xFEDCBA98n, BigInt("-18364758544493064719"), 0x0n);
testDiv(0xFEDCBA97n, 0xFEDCBA9876543210n, 0x0n);
testDiv(0xFEDCBA97n, 0xFEDCBA987654320Fn, 0x0n);
testDiv(0xFEDCBA97n, 0xFEDCBA98n, 0x0n);
testDiv(0xFEDCBA97n, 0xFEDCBA97n, 0x1n);
testDiv(0xFEDCBA97n, 0x1234n, 0xE0042n);
testDiv(0xFEDCBA97n, 0x3n, 0x54F43E32n);
testDiv(0xFEDCBA97n, 0x2n, 0x7F6E5D4Bn);
testDiv(0xFEDCBA97n, 0x1n, 0xFEDCBA97n);
testDiv(0x3n, 0xFEDCBA9876543210n, 0x0n);
testDiv(0x3n, 0xFEDCBA98n, 0x0n);
testDiv(0x3n, 0x1234n, 0x0n);
testDiv(0x3n, 0x3n, 0x1n);
testDiv(0x3n, 0x2n, 0x1n);
testDiv(0x3n, 0x1n, 0x3n);
testDiv(0x3n, BigInt("-2"), BigInt("-1"));
testDiv(0x3n, BigInt("-3"), BigInt("-1"));
testDiv(0x3n, BigInt("-4275878551"), 0x0n);
testDiv(0x3n, BigInt("-18364758544493064719"), 0x0n);
testDiv(0x2n, 0xFEDCBA98n, 0x0n);
testDiv(0x2n, 0xFEDCBA97n, 0x0n);
testDiv(0x2n, 0x3n, 0x0n);
testDiv(0x2n, 0x1n, 0x2n);
testDiv(0x2n, BigInt("-1"), BigInt("-2"));
testDiv(0x2n, BigInt("-2"), BigInt("-1"));
testDiv(0x2n, BigInt("-3"), 0x0n);
testDiv(0x1n, 0x1234n, 0x0n);
testDiv(0x1n, 0x3n, 0x0n);
testDiv(0x1n, 0x2n, 0x0n);
testDiv(0x1n, 0x1n, 0x1n);
testDiv(0x1n, BigInt("-1"), BigInt("-1"));
testDiv(0x1n, BigInt("-3"), 0x0n);
testDiv(0x1n, BigInt("-4660"), 0x0n);
testDiv(0x1n, BigInt("-18364758544493064719"), 0x0n);
testDiv(BigInt("-1"), 0xFEDCBA9876543210n, 0x0n);
testDiv(BigInt("-1"), 0xFEDCBA987654320Fn, 0x0n);
testDiv(BigInt("-1"), 0xFEDCBA98n, 0x0n);
testDiv(BigInt("-1"), 0xFEDCBA97n, 0x0n);
testDiv(BigInt("-1"), 0x3n, 0x0n);
testDiv(BigInt("-1"), 0x1n, BigInt("-1"));
testDiv(BigInt("-1"), BigInt("-3"), 0x0n);
testDiv(BigInt("-1"), BigInt("-4660"), 0x0n);
testDiv(BigInt("-1"), BigInt("-18364758544493064719"), 0x0n);
testDiv(BigInt("-2"), 0xFEDCBA9876543210n, 0x0n);
testDiv(BigInt("-3"), 0x3n, BigInt("-1"));
testDiv(BigInt("-3"), 0x2n, BigInt("-1"));
testDiv(BigInt("-3"), BigInt("-1"), 0x3n);
testDiv(BigInt("-3"), BigInt("-3"), 0x1n);
testDiv(BigInt("-3"), BigInt("-4660"), 0x0n);
testDiv(BigInt("-3"), BigInt("-4275878551"), 0x0n);
testDiv(BigInt("-3"), BigInt("-4275878552"), 0x0n);
testDiv(BigInt("-3"), BigInt("-18364758544493064720"), 0x0n);
testDiv(BigInt("-18364758544493064719"), 0xFEDCBA97n, BigInt("-4294967297"));
testDiv(BigInt("-18364758544493064719"), 0x1234n, BigInt("-3940935309977052"));
testDiv(BigInt("-18364758544493064719"), 0x3n, BigInt("-6121586181497688239"));
testDiv(BigInt("-18364758544493064719"), 0x2n, BigInt("-9182379272246532359"));
testDiv(BigInt("-18364758544493064719"), 0x1n, BigInt("-18364758544493064719"));
testDiv(BigInt("-18364758544493064719"), BigInt("-1"), 0xFEDCBA987654320Fn);
testDiv(BigInt("-18364758544493064719"), BigInt("-4275878551"), 0x100000001n);
testDiv(BigInt("-18364758544493064719"), BigInt("-18364758544493064719"), 0x1n);
testDiv(BigInt("-18364758544493064720"), 0xFEDCBA9876543210n, BigInt("-1"));
testDiv(BigInt("-18364758544493064720"), 0x1234n, BigInt("-3940935309977052"));
testDiv(BigInt("-18364758544493064720"), 0x3n, BigInt("-6121586181497688240"));
testDiv(BigInt("-18364758544493064720"), 0x2n, BigInt("-9182379272246532360"));
testDiv(BigInt("-18364758544493064720"), 0x1n, BigInt("-18364758544493064720"));
testDiv(BigInt("-18364758544493064720"), BigInt("-1"), 0xFEDCBA9876543210n);
testDiv(BigInt("-18364758544493064720"), BigInt("-3"), 0x54F43E32D21C10B0n);
testDiv(BigInt("-18364758544493064720"), BigInt("-4660"), 0xE0042813BE5DCn);
testDiv(BigInt("-18364758544493064720"), BigInt("-4275878552"), 0x100000000n);
testDiv(BigInt("-18364758544493064720"), BigInt("-18364758544493064720"), 0x1n);

// Test division by 0
try {
    let a = 102122311n / 0n;
} catch (e) {
    assert(e instanceof RangeError);
    assert(e.message == "0 is an invalid divisor value.");
}

