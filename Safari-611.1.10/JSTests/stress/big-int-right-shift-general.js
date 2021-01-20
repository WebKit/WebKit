// Copyright (C) 2017 Josh Wolfe. All rights reserved.
// Copyright (C) 2020 Apple. All rights reserved
// This code is governed by the BSD license found in the LICENSE file.

function testRightShift(left, right, expected)
{
    var result = left >> right;
    if (result !== expected)
        throw new Error("" + left + ", " + right + " resulted in " + result + " but expected " + expected);
}

for (var i = 0; i < 1000 ; ++i) {
    testRightShift(0n, 0n, 0n);
    testRightShift(0b101n, -1n, 0b1010n);
    testRightShift(0b101n, -2n, 0b10100n);
    testRightShift(0b101n, -3n, 0b101000n);
    testRightShift(0b101n, 1n, 0b10n);
    testRightShift(0b101n, 2n, 1n);
    testRightShift(0b101n, 3n, 0n);
    testRightShift(0n, -128n, 0n);
    testRightShift(0n, 128n, 0n);
    testRightShift(0x246n, 0n, 0x246n);
    testRightShift(0x246n, -127n, 0x12300000000000000000000000000000000n);
    testRightShift(0x246n, -128n, 0x24600000000000000000000000000000000n);
    testRightShift(0x246n, -129n, 0x48c00000000000000000000000000000000n);
    testRightShift(0x246n, 128n, 0n);
    testRightShift(0x123456789abcdef0fedcba9876543212345678n, -64n, 0x123456789abcdef0fedcba98765432123456780000000000000000n);
    testRightShift(0x123456789abcdef0fedcba9876543212345678n, -32n, 0x123456789abcdef0fedcba987654321234567800000000n);
    testRightShift(0x123456789abcdef0fedcba9876543212345678n, -16n, 0x123456789abcdef0fedcba98765432123456780000n);
    testRightShift(0x123456789abcdef0fedcba9876543212345678n, 0n, 0x123456789abcdef0fedcba9876543212345678n);
    testRightShift(0x123456789abcdef0fedcba9876543212345678n, 16n, 0x123456789abcdef0fedcba987654321234n);
    testRightShift(0x123456789abcdef0fedcba9876543212345678n, 32n, 0x123456789abcdef0fedcba98765432n);
    testRightShift(0x123456789abcdef0fedcba9876543212345678n, 64n, 0x123456789abcdef0fedcban);
    testRightShift(0x123456789abcdef0fedcba9876543212345678n, 127n, 0x2468acn);
    testRightShift(0x123456789abcdef0fedcba9876543212345678n, 128n, 0x123456n);
    testRightShift(0x123456789abcdef0fedcba9876543212345678n, 129n, 0x91a2bn);
    testRightShift(-5n, -1n, -0xan);
    testRightShift(-5n, -2n, -0x14n);
    testRightShift(-5n, -3n, -0x28n);
    testRightShift(-5n, 1n, -3n);
    testRightShift(-5n, 2n, -2n);
    testRightShift(-5n, 3n, -1n);
    testRightShift(-1n, -128n, -0x100000000000000000000000000000000n);
    testRightShift(-1n, 0n, -1n);
    testRightShift(-1n, 128n, -1n);
    testRightShift(-0x246n, 0n, -0x246n);
    testRightShift(-0x246n, -127n, -0x12300000000000000000000000000000000n);
    testRightShift(-0x246n, -128n, -0x24600000000000000000000000000000000n);
    testRightShift(-0x246n, -129n, -0x48c00000000000000000000000000000000n);
    testRightShift(-0x246n, 128n, -1n);
    testRightShift(-0x123456789abcdef0fedcba9876543212345678n, -64n, -0x123456789abcdef0fedcba98765432123456780000000000000000n);
    testRightShift(-0x123456789abcdef0fedcba9876543212345678n, -32n, -0x123456789abcdef0fedcba987654321234567800000000n);
    testRightShift(-0x123456789abcdef0fedcba9876543212345678n, -16n, -0x123456789abcdef0fedcba98765432123456780000n);
    testRightShift(-0x123456789abcdef0fedcba9876543212345678n, 0n, -0x123456789abcdef0fedcba9876543212345678n);
    testRightShift(-0x123456789abcdef0fedcba9876543212345678n, 16n, -0x123456789abcdef0fedcba987654321235n);
    testRightShift(-0x123456789abcdef0fedcba9876543212345678n, 32n, -0x123456789abcdef0fedcba98765433n);
    testRightShift(-0x123456789abcdef0fedcba9876543212345678n, 64n, -0x123456789abcdef0fedcbbn);
    testRightShift(-0x123456789abcdef0fedcba9876543212345678n, 127n, -0x2468adn);
    testRightShift(-0x123456789abcdef0fedcba9876543212345678n, 128n, -0x123457n);
    testRightShift(-0x123456789abcdef0fedcba9876543212345678n, 129n, -0x91a2cn);
}
