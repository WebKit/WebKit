// Copyright (C) 2017 Josh Wolfe. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

function assert(a) {
    if (!a)
        throw new Error("Bad assertion");
}

assert.sameValue = function (input, expected, message) {
    if (input !== expected)
        throw new Error(message);
}

assert.sameValue(0b00n & 0b00n, 0b00n, "0b00n & 0b00n === 0b00n");
assert.sameValue(0b00n & 0b01n, 0b00n, "0b00n & 0b01n === 0b00n");
assert.sameValue(0b01n & 0b00n, 0b00n, "0b01n & 0b00n === 0b00n");
assert.sameValue(0b00n & 0b10n, 0b00n, "0b00n & 0b10n === 0b00n");
assert.sameValue(0b10n & 0b00n, 0b00n, "0b10n & 0b00n === 0b00n");
assert.sameValue(0b00n & 0b11n, 0b00n, "0b00n & 0b11n === 0b00n");
assert.sameValue(0b11n & 0b00n, 0b00n, "0b11n & 0b00n === 0b00n");
assert.sameValue(0b01n & 0b01n, 0b01n, "0b01n & 0b01n === 0b01n");
assert.sameValue(0b01n & 0b10n, 0b00n, "0b01n & 0b10n === 0b00n");
assert.sameValue(0b10n & 0b01n, 0b00n, "0b10n & 0b01n === 0b00n");
assert.sameValue(0b01n & 0b11n, 0b01n, "0b01n & 0b11n === 0b01n");
assert.sameValue(0b11n & 0b01n, 0b01n, "0b11n & 0b01n === 0b01n");
assert.sameValue(0b10n & 0b10n, 0b10n, "0b10n & 0b10n === 0b10n");
assert.sameValue(0b10n & 0b11n, 0b10n, "0b10n & 0b11n === 0b10n");
assert.sameValue(0b11n & 0b10n, 0b10n, "0b11n & 0b10n === 0b10n");
assert.sameValue(0xffffffffn & 0n, 0n, "0xffffffffn & 0n === 0n");
assert.sameValue(0n & 0xffffffffn, 0n, "0n & 0xffffffffn === 0n");
assert.sameValue(0xffffffffn & 0xffffffffn, 0xffffffffn, "0xffffffffn & 0xffffffffn === 0xffffffffn");
assert.sameValue(0xffffffffffffffffn & 0n, 0n, "0xffffffffffffffffn & 0n === 0n");
assert.sameValue(0n & 0xffffffffffffffffn, 0n, "0n & 0xffffffffffffffffn === 0n");
assert.sameValue(0xffffffffffffffffn & 0xffffffffn, 0xffffffffn, "0xffffffffffffffffn & 0xffffffffn === 0xffffffffn");
assert.sameValue(0xffffffffn & 0xffffffffffffffffn, 0xffffffffn, "0xffffffffn & 0xffffffffffffffffn === 0xffffffffn");
assert.sameValue(
  0xffffffffffffffffn & 0xffffffffffffffffn, 0xffffffffffffffffn,
  "0xffffffffffffffffn & 0xffffffffffffffffn === 0xffffffffffffffffn");
assert.sameValue(
  0xbf2ed51ff75d380fd3be813ec6185780n & 0x4aabef2324cedff5387f1f65n, 0x42092803008e813400181700n,
  "0xbf2ed51ff75d380fd3be813ec6185780n & 0x4aabef2324cedff5387f1f65n === 0x42092803008e813400181700n");
assert.sameValue(
  0x4aabef2324cedff5387f1f65n & 0xbf2ed51ff75d380fd3be813ec6185780n, 0x42092803008e813400181700n,
  "0x4aabef2324cedff5387f1f65n & 0xbf2ed51ff75d380fd3be813ec6185780n === 0x42092803008e813400181700n");
assert.sameValue(0n & -1n, 0n, "0n & -1n === 0n");
assert.sameValue(-1n & 0n, 0n, "-1n & 0n === 0n");
assert.sameValue(0n & -2n, 0n, "0n & -2n === 0n");
assert.sameValue(-2n & 0n, 0n, "-2n & 0n === 0n");
assert.sameValue(1n & -2n, 0n, "1n & -2n === 0n");
assert.sameValue(-2n & 1n, 0n, "-2n & 1n === 0n");
assert.sameValue(2n & -2n, 2n, "2n & -2n === 2n");
assert.sameValue(-2n & 2n, 2n, "-2n & 2n === 2n");
assert.sameValue(2n & -3n, 0n, "2n & -3n === 0n");
assert.sameValue(-3n & 2n, 0n, "-3n & 2n === 0n");
assert.sameValue(-1n & -2n, -2n, "-1n & -2n === -2n");
assert.sameValue(-2n & -1n, -2n, "-2n & -1n === -2n");
assert.sameValue(-2n & -2n, -2n, "-2n & -2n === -2n");
assert.sameValue(-2n & -3n, -4n, "-2n & -3n === -4n");
assert.sameValue(-3n & -2n, -4n, "-3n & -2n === -4n");
assert.sameValue(0xffffffffn & -1n, 0xffffffffn, "0xffffffffn & -1n === 0xffffffffn");
assert.sameValue(-1n & 0xffffffffn, 0xffffffffn, "-1n & 0xffffffffn === 0xffffffffn");
assert.sameValue(0xffffffffffffffffn & -1n, 0xffffffffffffffffn, "0xffffffffffffffffn & -1n === 0xffffffffffffffffn");
assert.sameValue(-1n & 0xffffffffffffffffn, 0xffffffffffffffffn, "-1n & 0xffffffffffffffffn === 0xffffffffffffffffn");
assert.sameValue(
  0xbf2ed51ff75d380fd3be813ec6185780n & -0x4aabef2324cedff5387f1f65n, 0xbf2ed51fb554100cd330000ac6004080n,
  "0xbf2ed51ff75d380fd3be813ec6185780n & -0x4aabef2324cedff5387f1f65n === 0xbf2ed51fb554100cd330000ac6004080n");
assert.sameValue(
  -0x4aabef2324cedff5387f1f65n & 0xbf2ed51ff75d380fd3be813ec6185780n, 0xbf2ed51fb554100cd330000ac6004080n,
  "-0x4aabef2324cedff5387f1f65n & 0xbf2ed51ff75d380fd3be813ec6185780n === 0xbf2ed51fb554100cd330000ac6004080n");
assert.sameValue(
  -0xbf2ed51ff75d380fd3be813ec6185780n & 0x4aabef2324cedff5387f1f65n, 0x8a2c72024405ec138670800n,
  "-0xbf2ed51ff75d380fd3be813ec6185780n & 0x4aabef2324cedff5387f1f65n === 0x8a2c72024405ec138670800n");
assert.sameValue(
  0x4aabef2324cedff5387f1f65n & -0xbf2ed51ff75d380fd3be813ec6185780n, 0x8a2c72024405ec138670800n,
  "0x4aabef2324cedff5387f1f65n & -0xbf2ed51ff75d380fd3be813ec6185780n === 0x8a2c72024405ec138670800n");
assert.sameValue(
  -0xbf2ed51ff75d380fd3be813ec6185780n & -0x4aabef2324cedff5387f1f65n, -0xbf2ed51fffffff2ff7fedffffe7f5f80n,
  "-0xbf2ed51ff75d380fd3be813ec6185780n & -0x4aabef2324cedff5387f1f65n === -0xbf2ed51fffffff2ff7fedffffe7f5f80n");
assert.sameValue(
  -0x4aabef2324cedff5387f1f65n & -0xbf2ed51ff75d380fd3be813ec6185780n, -0xbf2ed51fffffff2ff7fedffffe7f5f80n,
  "-0x4aabef2324cedff5387f1f65n & -0xbf2ed51ff75d380fd3be813ec6185780n === -0xbf2ed51fffffff2ff7fedffffe7f5f80n");
assert.sameValue(-0xffffffffn & 0n, 0n, "-0xffffffffn & 0n === 0n");
assert.sameValue(0n & -0xffffffffn, 0n, "0n & -0xffffffffn === 0n");
assert.sameValue(
  -0xffffffffffffffffn & 0x10000000000000000n, 0x10000000000000000n,
  "-0xffffffffffffffffn & 0x10000000000000000n === 0x10000000000000000n");
assert.sameValue(
  0x10000000000000000n & -0xffffffffffffffffn, 0x10000000000000000n,
  "0x10000000000000000n & -0xffffffffffffffffn === 0x10000000000000000n");
assert.sameValue(
  -0xffffffffffffffffffffffffn & 0x10000000000000000n, 0n,
  "-0xffffffffffffffffffffffffn & 0x10000000000000000n === 0n");
assert.sameValue(
  0x10000000000000000n & -0xffffffffffffffffffffffffn, 0n,
  "0x10000000000000000n & -0xffffffffffffffffffffffffn === 0n");
