// Control group: flat base, non-constant 1-char needle.
// This exercises the same operationStringIndexOf but base->isRope() is false,
// so the new branch is a single not-taken check. Should show no regression.

let flat = "a".repeat(150000);
let needles = ["x", "y"];
let sum = 0;

function test(s, n) {
    return s.indexOf(n);
}
noInline(test);

for (let i = 0; i < 2e4; i++)
    sum += test(flat, needles[i & 1]);

if (sum !== -2e4)
    throw new Error("bad: " + sum);
