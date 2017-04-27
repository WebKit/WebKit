// This test should not crash.

let error = null;
try {
    // FIXME: We should not parse this as an "in" operation.
    let r = "prop" i\u006E 20;
} catch(e) {
    error = e;
}
if (error.message !== "20 is not an Object.")
    throw new Error("Bad");
