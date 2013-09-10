description("This test ensures that regeular expression literals are constants, and so persist over multiple executions");

for (var i = 0; i < 2; i++) {
    currentRegExp = /a/;
    if (i)
        shouldBeFalse("currentRegExp === lastRegExp");
    lastRegExp = currentRegExp;
}

function test1() {
    for (var i = 0; i < 2; i++) {
        currentRegExp = /a/;
        if (i)
            shouldBeFalse("currentRegExp === lastRegExp");
        lastRegExp = currentRegExp;
    }
}
test1();

function returnRegExpLiteral() { return /a/ }

shouldBeFalse("returnRegExpLiteral() === returnRegExpLiteral()");

function returnConditionalRegExpLiteral(first) {
    if (first)
        return /a/;
    return /a/;
}

shouldBeFalse("returnConditionalRegExpLiteral(true) === returnConditionalRegExpLiteral(true)");
shouldBeFalse("returnConditionalRegExpLiteral(false) === returnConditionalRegExpLiteral(false)");
shouldBeFalse("returnConditionalRegExpLiteral(true) === returnConditionalRegExpLiteral(false)");
returnRegExpLiteral().someAddedProperty = true;
shouldBeUndefined("returnRegExpLiteral().someAddedProperty");
