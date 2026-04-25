// needle is a non-constant 1-char string → DFG/FTL calls operationStringIndexOf,
// not the WithOneChar variant. Before the patch this unconditionally resolved the rope.
//
// The rope must be a single-level rope with resolved fibers so that tryFindOneChar
// can walk it without bailing out. `a + b + c` produces MakeRope(MakeRope(a,b),c)
// whose first fiber is itself a rope → immediate bailout. Use two resolved fibers.

let left  = "a".repeat(75000);
let right = "b".repeat(75000);

let needles = ["x", "y"];
let sum = 0;

function test(s, n) {
    return s.indexOf(n);
}
noInline(test);

for (let i = 0; i < 2e4; i++) {
    // Fresh 2-fiber rope each iteration. MakeRope is not LICM'd (ExitsForExceptions).
    let r = left + right;
    sum += test(r, needles[i & 1]);
}

if (sum !== -2e4)
    throw new Error("bad: " + sum);
