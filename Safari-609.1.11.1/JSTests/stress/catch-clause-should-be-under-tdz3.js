// This test should not crash.
var caughtReferenceError = false;
try {
    try { throw {}; } catch ({a = (print(a), b), b}) { }
} catch (e) {
    caughtReferenceError = true;
}

if (!caughtReferenceError)
    throw Error("Missing ReferenceError");
