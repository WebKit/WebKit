function test(dividends, modulus, count) {
    let acc = 0n;
    for (let i = 0; i < count; i++)
        acc ^= dividends[i & 15] % modulus;
    return acc;
}
noInline(test);

// 2^255 - 19, a 4-digit modulus on 64-bit. Repeating one divisor arms JSBigInt's cached
// multiplicative inverse, and 8-digit dividends keep every operation on the cachedMod path.
const modulus = (1n << 255n) - 19n;

const dividends = [];
let mix = 0x9e3779b97f4a7c15n;
for (let i = 0; i < 16; i++) {
    let value = 1n << 511n;
    for (let digit = 0; digit < 8; digit++) {
        mix = (mix * 6364136223846793005n + 1442695040888963407n) & 0xffffffffffffffffn;
        value |= mix << BigInt(64 * digit);
    }
    dividends.push(value);
}

let result = 0n;
for (let i = 0; i < 400; i++)
    result = test(dividends, modulus, 2000);

if (result !== 36824771070820514176995199531755782447069968531054165419255681446912271768444n)
    throw new Error("bad result: " + result);
