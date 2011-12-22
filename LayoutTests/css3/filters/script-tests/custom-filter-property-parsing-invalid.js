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
testInvalidFilterRule("Too many mesh sizes", "custom(none, 10 20 30)");
testInvalidFilterRule("Wrong mesh-box type - 'padding'", "custom(url(shader), padding)");
testInvalidFilterRule("Wrong mesh-box type - 'none'", "custom(url(shader), none)");
testInvalidFilterRule("More mesh-box types", "custom(url(shader), padding-box filter-box)");
testInvalidFilterRule("Wrong order for mesh-detached and mesh-box type", "custom(url(shader), detached padding-box)");
testInvalidFilterRule("Wrong order for mesh size and mesh-box type", "custom(url(shader), padding-box 10)");
testInvalidFilterRule("Too many mesh sizes with mesh-box type", "custom(url(shader), 20 30 40 padding-box)");
testInvalidFilterRule("Invalid comma between mesh-sizes", "custom(url(shader), 20, 40 padding-box)");
testInvalidFilterRule("Invalid comma before mesh type", "custom(url(shader), 20, padding-box)");
testInvalidFilterRule("No mesh parameter", "custom(url(shader), )");
testInvalidFilterRule("Wrong mesh type - correct identifier is 'detached'", "custom(url(shader), detach)");
testInvalidFilterRule("No mesh size", "custom(url(shader), , p1 2)");

testInvalidFilterRule("Negative single mesh size", "custom(url(shader), -10)");
testInvalidFilterRule("Negative both mesh sizes", "custom(url(shader), -10 -10)");
testInvalidFilterRule("Negative and positive mesh size", "custom(url(shader), -10 10)");
testInvalidFilterRule("Zero single mesh size", "custom(url(shader), 0)");
testInvalidFilterRule("Zero both mesh sizes", "custom(url(shader), 0 0)");
testInvalidFilterRule("0 and 1 mesh sizes", "custom(url(shader), 0 1)");

testInvalidFilterRule("Too many parameter values", "custom(url(shader), p1 1 2 3 4 5)");
testInvalidFilterRule("Invalid parameter types", "custom(url(shader), p1 1.0 2.0 'text')");

testInvalidFilterRule("No parameter value 1", "custom(url(shader), p1)");
testInvalidFilterRule("No parameter value 2", "custom(url(shader), p1, p2, p3)");

