function assert(condition, reason) {
    if (!condition)
        throw new Error(reason);
}

var ShouldHaveExecuted = true;
var ShouldNotHaveExecuted = false;

function checkBasicBlock(func, expr, expectation) {
    if (expectation === ShouldHaveExecuted)
        assert(hasBasicBlockExecuted(func, expr, "should have executed"));
    else
        assert(!hasBasicBlockExecuted(func, expr, "should not have executed"));
}
