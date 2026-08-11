// Copyright (C) 2017 Robin Templeton. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

function testOneMul(x, y, z) {
    let result = x * y;
    if (result !== z)
        throw new Error("Computing " + x + " * " + y + " resulted in " + result + " instead of the expected " + z);
}

function testMul(x, y, z) {
    testOneMul(x, y, z);
    testOneMul(y, x, z);
}

testMul(0xFEDCBA9876543210n, 0xFEDCBA9876543210n, 0xFDBAC097C8DC5ACCDEEC6CD7A44A4100n);
testMul(0xFEDCBA9876543210n, 0xFEDCBA98n, 0xFDBAC097530ECA86541D5980n);
testMul(0xFEDCBA9876543210n, 0x1234n, 0x121F49F49F49F49F4B40n);
testMul(0xFEDCBA9876543210n, 0x3n, 0x2FC962FC962FC9630n);
testMul(0xFEDCBA9876543210n, 0x2n, 0x1FDB97530ECA86420n);
testMul(0xFEDCBA9876543210n, 0x1n, 0xFEDCBA9876543210n);
testMul(0xFEDCBA9876543210n, 0x0n, 0x0n);
testMul(0xFEDCBA9876543210n, BigInt("-1"), BigInt("-18364758544493064720"));
testMul(0xFEDCBA9876543210n, BigInt("-2"), BigInt("-36729517088986129440"));
testMul(0xFEDCBA9876543210n, BigInt("-3"), BigInt("-55094275633479194160"));
testMul(0xFEDCBA9876543210n, BigInt("-4660"), BigInt("-85579774817337681595200"));
testMul(0xFEDCBA9876543210n, BigInt("-4275878551"), BigInt("-78525477154691874604502820720"));
testMul(0xFEDCBA987654320Fn, 0xFEDCBA987654320Fn, 0xFDBAC097C8DC5ACAE132F7A6B7A1DCE1n);
testMul(0xFEDCBA987654320Fn, 0xFEDCBA97n, 0xFDBAC09654320FECDEEC6CD9n);
testMul(0xFEDCBA987654320Fn, 0x3n, 0x2FC962FC962FC962Dn);
testMul(0xFEDCBA987654320Fn, 0x2n, 0x1FDB97530ECA8641En);
testMul(0xFEDCBA987654320Fn, 0x1n, 0xFEDCBA987654320Fn);
testMul(0xFEDCBA987654320Fn, 0x0n, 0x0n);
testMul(0xFEDCBA987654320Fn, BigInt("-1"), BigInt("-18364758544493064719"));
testMul(0xFEDCBA987654320Fn, BigInt("-2"), BigInt("-36729517088986129438"));
testMul(0xFEDCBA987654320Fn, BigInt("-3"), BigInt("-55094275633479194157"));
testMul(0xFEDCBA987654320Fn, BigInt("-4275878551"), BigInt("-78525477154691874600226942169"));
testMul(0xFEDCBA987654320Fn, BigInt("-18364758544493064720"), BigInt("-337264356397531028976608289633615613680"));
testMul(0xFEDCBA98n, 0xFEDCBA98n, 0xFDBAC096DD413A40n);
testMul(0xFEDCBA98n, 0x1234n, 0x121F49F496E0n);
testMul(0xFEDCBA98n, 0x3n, 0x2FC962FC8n);
testMul(0xFEDCBA98n, 0x2n, 0x1FDB97530n);
testMul(0xFEDCBA98n, 0x1n, 0xFEDCBA98n);
testMul(0xFEDCBA98n, 0x0n, 0x0n);
testMul(0xFEDCBA98n, BigInt("-1"), BigInt("-4275878552"));
testMul(0xFEDCBA98n, BigInt("-2"), BigInt("-8551757104"));
testMul(0xFEDCBA98n, BigInt("-3"), BigInt("-12827635656"));
testMul(0xFEDCBA98n, BigInt("-4275878551"), BigInt("-18283137387177738152"));
testMul(0xFEDCBA98n, BigInt("-18364758544493064720"), BigInt("-78525477173056633148995885440"));
testMul(0x3n, 0x3n, 0x9n);
testMul(0x3n, 0x2n, 0x6n);
testMul(0x3n, 0x1n, 0x3n);
testMul(0x3n, 0x0n, 0x0n);
testMul(0x3n, BigInt("-1"), BigInt("-3"));
testMul(0x3n, BigInt("-2"), BigInt("-6"));
testMul(0x3n, BigInt("-3"), BigInt("-9"));
testMul(0x3n, BigInt("-4660"), BigInt("-13980"));
testMul(0x3n, BigInt("-4275878552"), BigInt("-12827635656"));
testMul(0x3n, BigInt("-18364758544493064720"), BigInt("-55094275633479194160"));
testMul(0x0n, 0x0n, 0x0n);
testMul(0x0n, BigInt("-1"), 0x0n);
testMul(0x0n, BigInt("-2"), 0x0n);
testMul(0x0n, BigInt("-3"), 0x0n);
testMul(0x0n, BigInt("-4275878551"), 0x0n);
testMul(0x0n, BigInt("-18364758544493064719"), 0x0n);
testMul(BigInt("-1"), BigInt("-1"), 0x1n);
testMul(BigInt("-1"), BigInt("-2"), 0x2n);
testMul(BigInt("-1"), BigInt("-3"), 0x3n);
testMul(BigInt("-1"), BigInt("-4660"), 0x1234n);
testMul(BigInt("-1"), BigInt("-4275878551"), 0xFEDCBA97n);
testMul(BigInt("-1"), BigInt("-4275878552"), 0xFEDCBA98n);
testMul(BigInt("-1"), BigInt("-18364758544493064719"), 0xFEDCBA987654320Fn);
testMul(BigInt("-1"), BigInt("-18364758544493064720"), 0xFEDCBA9876543210n);
testMul(BigInt("-3"), BigInt("-3"), 0x9n);
testMul(BigInt("-3"), BigInt("-4660"), 0x369Cn);
testMul(BigInt("-3"), BigInt("-4275878551"), 0x2FC962FC5n);
testMul(BigInt("-3"), BigInt("-4275878552"), 0x2FC962FC8n);
testMul(BigInt("-3"), BigInt("-18364758544493064719"), 0x2FC962FC962FC962Dn);
testMul(BigInt("-3"), BigInt("-18364758544493064720"), 0x2FC962FC962FC9630n);
testMul(BigInt("-18364758544493064720"), BigInt("-18364758544493064720"), 0xFDBAC097C8DC5ACCDEEC6CD7A44A4100n);

