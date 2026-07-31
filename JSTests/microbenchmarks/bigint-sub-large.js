function test(xs, ys, count) {
    let acc = 0n;
    for (let i = 0; i < count; i++) {
        const j = i & 15;
        acc ^= xs[j] - ys[j];
    }
    return acc;
}
noInline(test);

// absoluteSub subtracts in place into the result BigInt up to maxInPlaceSubSize digits and falls
// back to a heap scratch buffer above it. 16 digits is the widest in-place shape, so this measures
// the carry chain rather than the allocation on the fallback path.
const DIGITS = 16;

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
for (let i = 0; i < 16; i++) {
    const a = next();
    const b = next();
    xs.push(a > b ? a : b);
    ys.push(a > b ? b : a);
}

let result = 0n;
for (let i = 0; i < 300; i++)
    result = test(xs, ys, 2000);

if (result !== 71891386329443767158707905664154015008911858965957789748353906080712861843644288382394549956830465877209120688058999672351174711181550616760704581702074425876824462754691389715053609118859481803311799700140829418062619418990174425401528671503883436142794272781610538878098138904814862589857296077057787658208n)
    throw new Error("bad result: " + result);
