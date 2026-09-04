function test(xs, ys, count) {
    let acc = 0n;
    for (let i = 0; i < count; i++) {
        const j = i & 7;
        acc ^= xs[j] * ys[j];
    }
    return acc;
}
noInline(test);

const LARGE_DIGITS = 1024;
const SMALL_DIGITS = 96;

const xs = [];
const ys = [];
let mix = 0x9e3779b97f4a7c15n;
function next(digits) {
    let value = 0n;
    for (let digit = 0; digit < digits; digit++) {
        mix = (mix * 6364136223846793005n + 1442695040888963407n) & 0xffffffffffffffffn;
        value |= mix << BigInt(64 * digit);
    }
    return value | (1n << BigInt(64 * digits - 1));
}
for (let i = 0; i < 8; i++) {
    xs.push(next(LARGE_DIGITS));
    ys.push(next(SMALL_DIGITS));
}

let result = 0n;
for (let i = 0; i < 10; i++)
    result = test(xs, ys, 1000);
