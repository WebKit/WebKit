// Regression test for bug 177423
let r1 = /\k</;

let didThrow = false;

try {
    let r2 = new RegExp("\\k<1>", "u");
    didThrow = false;
} catch(e) {
    didThrow = true;
}

if (!didThrow)
    throw("Trying to create a named capture reference that starts with a number should Throw");
