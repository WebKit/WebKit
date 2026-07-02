description("Test to make sure that document.all works properly with strict eq in the DFG");

var f = function(a, b) {
    return a === b

}
noInline(f);

var testObj = {};

// Warm up f to dfg the code before the masqueradesAsUndefined watchpoint fires
for (var i = 0; i < 1000; i++) {
    if (!f(testObj,testObj))
        testFailed("f(testObj,testObj) should have been true but got false");}



if (undefined == document.all) {
    debug("document.all was undefined");
    shouldBe("f(testObj, document.all)", "false");

    // Get JIT to recompile the jettisoned code after watchpoint fired
    for (var i = 0; i < 1000; i++) {
        if (!f(testObj,testObj))
            testFailed("f(testObj,testObj) should have been true but got false");
    }

    shouldBe("f(document.all, document.all)", "true");
}
