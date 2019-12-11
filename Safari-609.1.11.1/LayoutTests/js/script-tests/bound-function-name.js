description("Bound Function Names");

function assert(b, text) {
    if (b)
        testPassed(text);
    else
        testFailed(`Bad result: ${text}`);

}
assert((function() {}).bind().name === "bound ", "Anonymous function bound name.");
assert((function foo() {}).bind().name === "bound foo", "Function bound name should be foo.");

function bar() { }
assert(bar.bind().name === "bound bar", "Function bound name should be bar.");
assert(bar.bind().bind().name === "bound bound bar", "Function double bound name should be bar.");

debug("Test InternalFunction names.");
assert(Error.bind().name === "bound Error", "Function bound name should be Error.");
assert(Function.bind().name === "bound Function", "Function bound name should be Function.");
