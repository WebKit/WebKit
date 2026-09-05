const re = /^[\p{L}--[A-Z]]+$/v;

function isLowerLetter(s) {
    return re.test(s);
}
noInline(isLowerLetter);

const words = ["alpha", "beta", "Gamma", "delta", "epsilon", "zeta2", "eta", "theta"];
let count = 0;
for (let i = 0; i < 2e6; ++i) {
    if (isLowerLetter(words[i & 7]))
        count++;
}
if (count !== 2e6 * 6 / 8)
    throw new Error("bad count: " + count);
