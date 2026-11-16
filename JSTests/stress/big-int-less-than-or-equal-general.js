// Copyright (C) 2017 Josh Wolfe. All rights reserved.
// Copyright (C) 2017 Robin Templeton. All rights reserved.
// Copyright (C) 2018 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

function assert(v, e, m) {
    if (v !== e)
        throw new Error(m);
}

assert(0n <= 0n, true, "0n <= 0n");
assert(1n <= 1n, true, "1n <= 1n");
assert(BigInt("-1") <= BigInt("-1"), true, "-1n <= -1n");
assert(0n <= BigInt("-0"), true, "0n <= -0n");
assert(BigInt("-0") <= 0n, true, "-0n <= 0n");
assert(0n <= 1n, true, "0n <= 1n");
assert(1n <= 0n, false, "1n <= 0n");
assert(0n <= BigInt("-1"), false, "0n <= -1n");
assert(BigInt("-1") <= 0n, true, "-1n <= 0n");
assert(1n <= BigInt("-1"), false, "1n <= -1n");
assert(BigInt("-1") <= 1n, true, "-1n <= 1n");
assert(0x1fffffffffffff01n <= 0x1fffffffffffff02n, true, "0x1fffffffffffff01n <= 0x1fffffffffffff02n");
assert(0x1fffffffffffff02n <= 0x1fffffffffffff01n, false, "0x1fffffffffffff02n <= 0x1fffffffffffff01n");
assert(BigInt("-2305843009213693697") <= BigInt("-2305843009213693698"), false, "-2305843009213693697n <= -2305843009213693698n");
assert(BigInt("-2305843009213693698") <= BigInt("-2305843009213693697"), true, "-2305843009213693698n <= -2305843009213693697n");
assert(0x10000000000000000n <= 0n, false, "0x10000000000000000n <= 0n");
assert(0n <= 0x10000000000000000n, true, "0n <= 0x10000000000000000n");
assert(0x10000000000000000n <= 1n, false, "0x10000000000000000n <= 1n");
assert(1n <= 0x10000000000000000n, true, "1n <= 0x10000000000000000n");
assert(0x10000000000000000n <= BigInt("-1"), false, "0x10000000000000000n <= -1n");
assert(BigInt("-1") <= 0x10000000000000000n, true, "-1n <= 0x10000000000000000n");
assert(0x10000000000000001n <= 0n, false, "0x10000000000000001n <= 0n");
assert(0n <= 0x10000000000000001n, true, "0n <= 0x10000000000000001n");
assert(BigInt("-18446744073709551616") <= 0n, true, "-18446744073709551616n <= 0n");
assert(0n <= BigInt("-18446744073709551616"), false, "0n <= -18446744073709551616n");
assert(BigInt("-18446744073709551616") <= 1n, true, "-18446744073709551616n <= 1n");
assert(1n <= BigInt("-18446744073709551616"), false, "1n <= -18446744073709551616n");
assert(BigInt("-18446744073709551616") <= BigInt("-1"), true, "-18446744073709551616n <= -1n");
assert(BigInt("-1") <= BigInt("-18446744073709551616"), false, "-1n <= -18446744073709551616n");
assert(BigInt("-18446744073709551617") <= 0n, true, "-18446744073709551617n <= 0n");
assert(0n <= BigInt("-18446744073709551617"), false, "0n <= -18446744073709551617n");
assert(0x10000000000000000n <= 0x100000000n, false, "0x10000000000000000n <= 0x100000000n");
assert(0x100000000n <= 0x10000000000000000n, true, "0x100000000n <= 0x10000000000000000n");

// BigInt - String

assert(0n <= "0", true, "0n <= '0'");
assert("0" <= 0n, true, "'0' <= 0n");
assert(0n <= "1", true, "0n <= '1'");
assert("0" <= 1n, true, "'0' <= 1n");
assert(1n <= "0", false, "1n <= '0'");
assert("1" <= 0n, false, "'1' <= 0n");
assert(0n <= "", true, "0n <= ''");
assert("" <= 0n, true, "'' <= 0n");
assert(0n <= "1", true, "0n <= '1'");
assert("" <= 1n, true, "'' <= 1n");
assert(1n <= "", false, "1n <= ''");
assert("1" <= 0n, false, "'1' <= 0n");
assert(1n <= "1", true, "1n <= '1'");
assert("1" <= 1n, true, "'1' <= 1n");
assert(1n <= "-1", false, "1n <= '-1'");
assert("1" <= BigInt("-1"), false, "'1' <= -1n");
assert(BigInt("-1") <= "1", true, "-1n <= '1'");
assert("-1" <= 1n, true, "'-1' <= 1n");
assert(BigInt("-1") <= "-1", true, "-1n <= '-1'");
assert("-1" <= BigInt("-1"), true, "'-1' <= -1n");
assert(9007199254740993n <= "9007199254740992", false, "9007199254740993n <= '9007199254740992'");
assert("9007199254740993" <= 9007199254740992n, false, "'9007199254740993' <= 9007199254740992n");
assert(BigInt("-9007199254740992") <= "-9007199254740993", false, "-9007199254740992n <= '-9007199254740993'");
assert("-9007199254740992" <= BigInt("-9007199254740993"), false, "'-9007199254740992' <= -9007199254740993n");
assert("0x10" <= 3n, false, "'0x10' <= 3n");
assert("0x10" <= 2n, false, "'0x10' <= 2n");
assert("0x10" <= 1n, false, "'0x10' <= 1n");
assert("0o10" <= 7n, false, "'0o10' <= 7n");
assert("0o10" <= 8n, true, "'0o10' <= 8n");
assert("0o10" <= 9n, true, "'0o10' <= 9n");
assert("0b10" <= 3n, true, "'0b10' <= 3n");
assert("0b10" <= 2n, true, "'0b10' <= 2n");
assert("0b10" <= 1n, false, "'0b10' <= 1n");

// Invalid String

assert("b10" <= 2n, false, "'b10' > 2n");
assert("bbb10" <= 2n, false, "'bbb10' > 2n");

// BigInt - Number

assert(0n <= 0, true, "0n <= 0");
assert(0 <= 0n, true, "0 <= 0n");
assert(0n <= -0, true, "0n <= -0");
assert(-0 <= 0n, true, "-0 <= 0n");
assert(0n <= 0.000000000001, true, "0n <= 0.000000000001");
assert(0.000000000001 <= 0n, false, "0.000000000001 <= 0n");
assert(0n <= 1, true, "0n <= 1");
assert(1 <= 0n, false, "1 <= 0n");
assert(1n <= 0, false, "1n <= 0");
assert(0 <= 1n, true, "0 <= 1n");
assert(1n <= 0.999999999999, false, "1n <= 0.999999999999");
assert(0.999999999999 <= 1n, true, "0.999999999999 <= 1n");
assert(1n <= 1, true, "1n <= 1");
assert(1 <= 1n, true, "1 <= 1n");
assert(0n <= Number.MIN_VALUE, true, "0n <= Number.MIN_VALUE");
assert(Number.MIN_VALUE <= 0n, false, "Number.MIN_VALUE <= 0n");
assert(0n <= -Number.MIN_VALUE, false, "0n <= -Number.MIN_VALUE");
assert(-Number.MIN_VALUE <= 0n, true, "-Number.MIN_VALUE <= 0n");
assert(BigInt("-10") <= Number.MIN_VALUE, true, "-10n <= Number.MIN_VALUE");
assert(Number.MIN_VALUE <= BigInt("-10"), false, "Number.MIN_VALUE <= -10n");
assert(1n <= Number.MAX_VALUE, true, "1n <= Number.MAX_VALUE");
assert(Number.MAX_VALUE <= 1n, false, "Number.MAX_VALUE <= 1n");
assert(1n <= -Number.MAX_VALUE, false, "1n <= -Number.MAX_VALUE");
assert(-Number.MAX_VALUE <= 1n, true, "-Number.MAX_VALUE <= 1n");
assert(0xfffffffffffff7ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffn <= Number.MAX_VALUE, true, "0xfffffffffffff7ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffn <= Number.MAX_VALUE");
assert(Number.MAX_VALUE <= 0xfffffffffffff7ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffn, false, "Number.MAX_VALUE <= 0xfffffffffffff7ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffn");
assert(0xfffffffffffff800000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001n <= Number.MAX_VALUE, false, "0xfffffffffffff800000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001n <= Number.MAX_VALUE");
assert(Number.MAX_VALUE <= 0xfffffffffffff800000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001n, true, "Number.MAX_VALUE <= 0xfffffffffffff800000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001n");
assert(1n <= Infinity, true, "1n <= Infinity");
assert(Infinity <= 1n, false, "Infinity <= 1n");
assert(BigInt("-1") <= Infinity, true, "-1n <= Infinity");
assert(Infinity <= BigInt("-1"), false, "Infinity <= -1n");
assert(1n <= -Infinity, false, "1n <= -Infinity");
assert(-Infinity <= 1n, true, "-Infinity <= 1n");
assert(BigInt("-1") <= -Infinity, false, "-1n <= -Infinity");
assert(-Infinity <= BigInt("-1"), true, "-Infinity <= -1n");
assert(0n <= NaN, false, "0n <= NaN");
assert(NaN <= 0n, false, "NaN <= 0n");

// BigInt - Boolean

assert(false <= 1n, true, "false <= 1n");
assert(1n <= false, false, "1n <= false");
assert(false <= 0n, true, "false <= 0n");
assert(0n <= false, true, "0n <= false");
assert(true <= 1n, true, "true <= 1n");
assert(1n <= true, true, "1n <= true");
assert(true <= 2n, true, "true <= 2n");
assert(2n <= true, false, "2n <= true");

// BigInt - Symbol

try {
    1n <= Symbol("1");
    assert(false, true, "Comparison with Symbol shoud throw TypeError, but executed without exception");
} catch(e) {
    assert(e instanceof TypeError, true, "Comparison with Symbol shoud throw TypeError, but throwed something else");
}

