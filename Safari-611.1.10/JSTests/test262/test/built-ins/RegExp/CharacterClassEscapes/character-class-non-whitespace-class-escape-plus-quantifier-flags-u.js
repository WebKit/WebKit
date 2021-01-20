// Copyright (C) 2018 Leo Balter.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: prod-CharacterClassEscape
description: >
    Compare range for non-whitespace class escape \S+ with flags ug
info: |
    This is a generated test. Please check out
    https://github.com/bocoup/test262-regexp-generator
    for any changes.

    CharacterClassEscape[U] ::
        d
        D
        s
        S
        w
        W

    21.2.2.12 CharacterClassEscape

    The production CharacterClassEscape :: d evaluates as follows:
        Return the ten-element set of characters containing the characters 0 through 9 inclusive.
    The production CharacterClassEscape :: D evaluates as follows:
        Return the set of all characters not included in the set returned by CharacterClassEscape :: d.
    The production CharacterClassEscape :: s evaluates as follows:
        Return the set of characters containing the characters that are on the right-hand side of
        the WhiteSpace or LineTerminator productions.
    The production CharacterClassEscape :: S evaluates as follows:
        Return the set of all characters not included in the set returned by CharacterClassEscape :: s.
    The production CharacterClassEscape :: w evaluates as follows:
        Return the set of all characters returned by WordCharacters().
    The production CharacterClassEscape :: W evaluates as follows:
        Return the set of all characters not included in the set returned by CharacterClassEscape :: w.
features: [String.fromCodePoint]
includes: [regExpUtils.js]
---*/

const str = buildString({
    loneCodePoints: [],
    ranges: [
        [0x00DC00, 0x00DFFF],
        [0x000000, 0x000008],
        [0x00000E, 0x00001F],
        [0x000021, 0x00009F],
        [0x0000A1, 0x00167F],
        [0x001681, 0x001FFF],
        [0x00200B, 0x002027],
        [0x00202A, 0x00202E],
        [0x002030, 0x00205E],
        [0x002060, 0x002FFF],
        [0x003001, 0x00DBFF],
        [0x00E000, 0x00FEFE],
        [0x00FF00, 0x10FFFF],
    ],
});

const re = /\S+/ug;

const errors = [];

if (!re.test(str)) {
    // Error, let's find out where
    for (const char of str) {
        if (!re.test(char)) {
            errors.push('0x' + char.codePointAt(0).toString(16));
        }
    }
}

assert.sameValue(
    errors.length,
    0,
    'Expected matching code points, but received: ' + errors.join(',')
);
