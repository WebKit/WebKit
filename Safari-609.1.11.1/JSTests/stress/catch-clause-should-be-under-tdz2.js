// This test should not crash.
var caughtReferenceError = false;
try {
    try { throw []; } catch ({c = new class extends C {}, constructor: C}) { }
} catch (e) {
    caughtReferenceError = true;
}

if (!caughtReferenceError)
    throw Error("Missing ReferenceError");
