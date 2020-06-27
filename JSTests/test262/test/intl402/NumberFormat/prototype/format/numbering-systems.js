// Copyright 2012 Google Inc.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: table-numbering-system-digits
description: >
    Tests that Intl.NumberFormat.prototype.format supports all
    numbering systems with simple digit mappings.
author: Roozbeh Pournader
---*/

const numberingSystems = {
    adlm: 0x1E950,
    ahom: 0x11730,
    arab: 0x0660,
    arabext: 0x06F0,
    bali: 0x1B50,
    beng: 0x09E6,
    bhks: 0x11C50,
    brah: 0x11066,
    cakm: 0x11136,
    cham: 0xAA50,
    deva: 0x0966,
    diak: 0x11950,
    fullwide: 0xFF10,
    gong: 0x11DA0,
    gonm: 0x11D50,
    gujr: 0x0AE6,
    guru: 0x0A66,
    hanidec: [0x3007, 0x4E00, 0x4E8C, 0x4E09, 0x56DB,
              0x4E94, 0x516D, 0x4E03, 0x516B, 0x4E5D],
    hmng: 0x16B50,
    hmnp: 0x1E140,
    java: 0xA9D0,
    kali: 0xA900,
    khmr: 0x17E0,
    knda: 0x0CE6,
    lana: 0x1A80,
    lanatham: 0x1A90,
    laoo: 0x0ED0,
    latn: 0x0030,
    lepc: 0x1C40,
    limb: 0x1946,
    mathbold: 0x1D7CE,
    mathdbl: 0x1D7D8,
    mathmono: 0x1D7F6,
    mathsanb: 0x1D7EC,
    mathsans: 0x1D7E2,
    mlym: 0x0D66,
    modi: 0x11650,
    mong: 0x1810,
    mroo: 0x16A60,
    mtei: 0xABF0,
    mymr: 0x1040,
    mymrshan: 0x1090,
    mymrtlng: 0xA9F0,
    newa: 0x11450,
    nkoo: 0x07C0,
    olck: 0x1C50,
    orya: 0x0B66,
    osma: 0x104A0,
    rohg: 0x10D30,
    saur: 0xA8D0,
    segment: 0x1FBF0,
    shrd: 0x111D0,
    sind: 0x112F0,
    sinh: 0x0DE6,
    sora: 0x110F0,
    sund: 0x1BB0,
    takr: 0x116C0,
    talu: 0x19D0,
    tamldec: 0x0BE6,
    telu: 0x0C66,
    thai: 0x0E50,
    tibt: 0x0F20,
    tirh: 0x114D0,
    vaii: 0xA620,
    wara: 0x118E0,
    wcho: 0x1E2F0,
};

for (let [numberingSystem, digitList] of Object.entries(numberingSystems)) {
    if (typeof digitList === 'number') {
        let zeroCode = digitList;
        digitList = [];
        for (let i = 0; i <= 9; ++i) {
            digitList[i] = zeroCode + i;
        }
    }

    assert.sameValue(digitList.length, 10);

    let nf = new Intl.NumberFormat(undefined, {numberingSystem});

    for (let i = 0; i <= 9; ++i) {
        assert.sameValue(nf.format(i), String.fromCodePoint(digitList[i]),
                         `numberingSystem: ${numberingSystem}, digit: ${i}`);
    }
}
