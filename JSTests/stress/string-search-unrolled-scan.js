function naiveIndexOf(s, p, from) {
    outer: for (let i = from; i + p.length <= s.length; ++i) {
        for (let j = 0; j < p.length; ++j)
            if (s.charCodeAt(i + j) !== p.charCodeAt(j)) continue outer;
        return i;
    }
    return -1;
}
let seed = 1;
function rnd(n) { seed = (seed * 1103515245 + 12345) & 0x7fffffff; return seed % n; }
const alphabets = ["ab#>", "abcdefgh#<>[]", "あいab#>"];
for (let iter = 0; iter < Math.min(testLoopCount, 1500); ++iter) {
    const alpha = alphabets[iter % alphabets.length];
    let len = rnd(400);
    let s = "";
    for (let i = 0; i < len; ++i) s += alpha[rnd(alpha.length)];
    let plen = 2 + rnd(20);
    let p;
    if (rnd(2) && len > plen) { const at = rnd(len - plen); p = s.substring(at, at + plen); }
    else { p = ""; for (let i = 0; i < plen; ++i) p += alpha[rnd(alpha.length)]; }
    const from = rnd(len + 1);
    const expected = naiveIndexOf(s, p, from);
    const actual = s.indexOf(p, from);
    if (actual !== expected) throw new Error(`indexOf mismatch ${JSON.stringify(s)} ${JSON.stringify(p)} ${from}: ${actual} vs ${expected}`);
    const parts = s.split(p);
    let rebuilt = [], pos = 0, k;
    while ((k = naiveIndexOf(s, p, pos)) !== -1) { rebuilt.push(s.substring(pos, k)); pos = k + p.length; }
    rebuilt.push(s.substring(pos));
    if (parts.length !== rebuilt.length || parts.some((x, i) => x !== rebuilt[i])) throw new Error("split mismatch");
}
