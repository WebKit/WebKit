//@ skip if $model == "Apple Watch Series 3"

function fromCodePointLatin1(start, count) {
    let result = 0;
    for (let i = 0; i < count; ++i)
        result += String.fromCodePoint((start + i) & 0xFF).length;
    return result;
}
noInline(fromCodePointLatin1);

function fromCodePointBMP(start, count) {
    let result = 0;
    for (let i = 0; i < count; ++i)
        result += String.fromCodePoint(0x3000 + ((start + i) & 0xFF)).length;
    return result;
}
noInline(fromCodePointBMP);

function fromCodePointMixed(start, count) {
    let result = 0;
    for (let i = 0; i < count; ++i) {
        const cp = ((start + i) & 0x1) ? 0x41 : 0x1F600;
        result += String.fromCodePoint(cp).length;
    }
    return result;
}
noInline(fromCodePointMixed);

let total = 0;
for (let i = 0; i < 5000; ++i) {
    total += fromCodePointLatin1(i, 200);
    total += fromCodePointBMP(i, 200);
    total += fromCodePointMixed(i, 200);
}

if (total !== 5000 * (200 + 200 + 300))
    throw new Error(`bad total: ${total}`);
