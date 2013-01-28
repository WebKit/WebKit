// Requires custom-filter-parsing-common.js.

description("Test at-rule parsing for @-webkit-filter.");

// These have to be global for the test helpers to see them.
var cssRule, insertRuleException;

function testInvalidFilterAtRule(description, rule)
{
    debug("\n" + description + "\n" + rule);

    try {
        stylesheet.insertRule(rule, 0);
        testFailed("\"" + rule + "\" did not throw a syntax error.");
    } catch (e) {
        insertRuleException = e;
        shouldBeTrue("insertRuleException instanceof DOMException");
        shouldEvaluateTo("insertRuleException.code", DOMException.SYNTAX_ERR)
    }
}

testInvalidFilterAtRule("Unprefixed rule.", "@filter my-filter { }");

testInvalidFilterAtRule("Missing filter name identifier.", "@-webkit-filter { }");
testInvalidFilterAtRule("Filter name as string.", "@-webkit-filter 'my-filter' { }");
testInvalidFilterAtRule("Filter name as number.", "@-webkit-filter 123 { }");

testInvalidFilterAtRule("Missing rule body.", "@-webkit-filter my-filter");
testInvalidFilterAtRule("Missing opening brace.", "@-webkit-filter my-filter }");
testInvalidFilterAtRule("Missing closing brace.", "@-webkit-filter my-filter {");
