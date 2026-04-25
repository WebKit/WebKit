function assert(cond, msg = "") {
    if (!cond)
        throw new Error(msg);
}
noInline(assert);

function shouldThrowSyntaxError(str) {
    var hadError = false;
    try {
        eval(str);
    } catch (e) {
        if (e instanceof SyntaxError)
            hadError = true;
    }
    assert(hadError, "Did not throw syntax error");
}
noInline(shouldThrowSyntaxError);

shouldThrowSyntaxError("var f = new Function('}{')");
shouldThrowSyntaxError("var f = new Function('}}{{')");

var GeneratorFunction = function*(){}.constructor;
shouldThrowSyntaxError("var f = new GeneratorFunction('}{')");
shouldThrowSyntaxError("var f = new GeneratorFunction('}}{{')");
