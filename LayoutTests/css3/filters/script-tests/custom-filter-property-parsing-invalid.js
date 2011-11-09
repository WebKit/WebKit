description("Test the parsing of custom() function of the -webkit-filter property.");

// These have to be global for the test helpers to see them.
var stylesheet, cssRule, declaration;

function testInvalidFilterRule(description, rule)
{
    debug("");
    debug(description + " : " + rule);

    stylesheet = document.styleSheets.item(0);
    stylesheet.insertRule("body { -webkit-filter: " + rule + "; }", 0);
    cssRule = stylesheet.cssRules.item(0);
  
    shouldBe("cssRule.type", "1");

    declaration = cssRule.style;
    shouldBe("declaration.length", "0");
    shouldBe("declaration.getPropertyValue('-webkit-filter')", "null");
}

testInvalidFilterRule("Empty function", "custom()");
testInvalidFilterRule("No values 1", "custom(,)");
testInvalidFilterRule("No values 2", "custom(,,)");

testInvalidFilterRule("Too many shaders 1", "custom(url(shader1) url(shader2) url(shader2))");
testInvalidFilterRule("Too many shaders 2", "custom(none none none)");
testInvalidFilterRule("Too many shaders 3", "custom(none url(shader1) url(shader2) url(shader2))");

testInvalidFilterRule("No shader", "custom(none, 10 20)");
testInvalidFilterRule("To many mesh sizes", "custom(none, 10 20 30)");
testInvalidFilterRule("Wrong mesh type", "custom(url(shader), detach)");
testInvalidFilterRule("No mesh size", "custom(url(shader), , p1 2)");

testInvalidFilterRule("To many parameter values", "custom(url(shader), p1 1 2 3 4 5)");
testInvalidFilterRule("Invalid parameter types", "custom(url(shader), p1 1.0 2.0 'text')");

testInvalidFilterRule("No parameter value 1", "custom(url(shader), p1)");
testInvalidFilterRule("No parameter value 2", "custom(url(shader), p1, p2, p3)");

