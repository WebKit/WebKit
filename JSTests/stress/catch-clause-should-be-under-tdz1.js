// This test should not crash.
var caughtReferenceError = false;
try {
    try { throw [void 0]; } catch ([{constructor} = new constructor]) { }
} catch (e) {
    caughtReferenceError = true;
}

if (!caughtReferenceError)
    throw Error("Missing ReferenceError");
