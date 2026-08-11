// Same shape as string-indexof-rope-dynamic-onechar.js but needle HITS early.
// Baseline: flatten 150KB then find at index ~100.
// Patched: scan ~100 bytes in fiber[0], return — no flatten.
//
// left/right must themselves be resolved so the top-level rope's fibers are
// plain strings (tryFindOneChar bails on nested rope fibers). "a".repeat(n)
// returns a flat string; "x"+flat makes a rope, so we resolve it once up front.

let left  = "x" + "a".repeat(74999);
left.indexOf("never-matches"); // multi-char search → view() → resolves left in place
let right = "b".repeat(75000);

let needles = ["x", "x"];
let sum = 0;

function test(s, n) {
    return s.indexOf(n);
}
noInline(test);

for (let i = 0; i < 2e4; i++) {
    let r = left + right;
    sum += test(r, needles[i & 1]);
}

if (sum !== 0)
    throw new Error("bad: " + sum);
