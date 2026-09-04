function test(xs, ys, count) {
    let acc = 0n;
    for (let i = 0; i < count; i++) {
        const j = i & 7;
        acc ^= xs[j] * ys[j];
    }
    return acc;
}
noInline(test);

const DIGITS = 256;

const xs = [];
const ys = [];
let mix = 0x9e3779b97f4a7c15n;
function next() {
    let value = 0n;
    for (let digit = 0; digit < DIGITS; digit++) {
        mix = (mix * 6364136223846793005n + 1442695040888963407n) & 0xffffffffffffffffn;
        value |= mix << BigInt(64 * digit);
    }
    return value | (1n << BigInt(64 * DIGITS - 1));
}
for (let i = 0; i < 8; i++) {
    xs.push(next());
    ys.push(next());
}

let result = 0n;
for (let i = 0; i < 10; i++)
    result = test(xs, ys, 1000);
