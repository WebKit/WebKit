// This test should not crash.
var caughtReferenceError = false;
try {
    while(1) try {throw {}} catch({a=({}={__proto__}), __proto__}){}
} catch (e) {
    caughtReferenceError = true;
}

if (!caughtReferenceError)
    throw Error("Missing ReferenceError");
