// This test should not crash.
var caughtReferenceError = false;
try {
    function* m(){ try {throw [void 0]} catch ([c = (yield c)]) {} }
    [...m()]
} catch (e) {
    caughtReferenceError = true;
}

if (!caughtReferenceError)
    throw Error("Missing ReferenceError");
