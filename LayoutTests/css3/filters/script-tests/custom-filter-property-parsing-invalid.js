description("Test the parsing of custom() function of the -webkit-filter property.");

// These have to be global for the test helpers to see them.
var cssRule, declaration;

function testInvalidFilterRule(description, rule)
{
    debug("");
    debug(description + " : " + rule);

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

testInvalidFilterRule("Mix function as vertex shader", "custom(mix(url(shader)))");
testInvalidFilterRule("Mix function with wrong name", "custom(none fn(url(shader)))");
testInvalidFilterRule("Mix function with 0 args", "custom(none mix())");
testInvalidFilterRule("Mix function with first arg not a URI", "custom(none mix(none))");
testInvalidFilterRule("Mix function with second arg not a blend mode or alpha compositing mode", "custom(none mix(url(shader) 0))");
testInvalidFilterRule("Mix function with third arg not a blend mode or alpha compositing mode", "custom(none mix(url(shader) normal 0))");
testInvalidFilterRule("Mix function with two blend modes", "custom(none mix(url(shader) multiply screen))");
testInvalidFilterRule("Mix function with two alpha compositing modes", "custom(none mix(url(shader) source-over destination-over))");
testInvalidFilterRule("Mix function with alpha compositing mode 'plus-darker', which should only apply to -webkit-background-composite", "custom(none mix(url(shader) plus-darker))");
testInvalidFilterRule("Mix function with alpha compositing mode 'plus-lighter', which should only apply to -webkit-background-composite", "custom(none mix(url(shader) plus-lighter))");
testInvalidFilterRule("Mix function with alpha compositing mode 'highlight', which should only apply to -webkit-background-composite", "custom(none mix(url(shader) highlight))");
testInvalidFilterRule("Mix function with 4 args", "custom(none mix(url(shader) multiply clear normal))");
testInvalidFilterRule("Mix function with comma separators", "custom(none mix(url(shader), multiply, clear))");
testInvalidFilterRule("Mix function with comma terminator", "custom(none mix(url(shader), multiply clear,))");
testInvalidFilterRule("Mix function with one comma", "custom(none mix(,))");

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

testInvalidFilterRule("One invalid transform", "custom(none url(shader), someId invalid_rotate(0deg))");
testInvalidFilterRule("Multiple invalid transforms", "custom(none url(shader), someId invalid_rotate(0deg) invalid_perspective(0))");
testInvalidFilterRule("Invalid transform between valid ones", "custom(none url(shader), someId rotate(0deg) invalid_rotate(0deg) perspective(0))");
testInvalidFilterRule("Valid transform between invalid ones", "custom(none url(shader), someId invalid_rotate(0deg) perspective(0) another_invalid(0))");
testInvalidFilterRule("Valid transform without leading comma", "custom(none url(shader) someId perspective(0))");
testInvalidFilterRule("Valid transform with trailing comma", "custom(none url(shader), someId perspective(0),)");
testInvalidFilterRule("Valid transform with trailing comma and without leading comma", "custom(none url(shader) someId perspective(0),)");
testInvalidFilterRule("Invalid transform with trailing comma", "custom(none url(shader), someId invalid_rotate(0deg),)");
testInvalidFilterRule("Invalid transform without leading comma", "custom(none url(shader) someId invalid_rotate(0deg))");
testInvalidFilterRule("Empty transform (only the id)", "custom(none url(shader), someId)");
testInvalidFilterRule("Empty transform (without the id)", "custom(none url(shader),)");
testInvalidFilterRule("Empty transform (two empty commas)", "custom(none url(shader),,)");
testInvalidFilterRule("Valid transform with invalid characters", "custom(none url(shader),someId rotate(0deg) *.-,)");

testInvalidFilterRule("Empty array()", "custom(none url(shader), id array())");
testInvalidFilterRule("One comma in array()", "custom(none url(shader), id array(,))");
testInvalidFilterRule("Multile commas in array()", "custom(none url(shader), id array(,,,))");
testInvalidFilterRule("Multiple commas with a value in array()", "custom(none url(shader), id array(,,1,))");
testInvalidFilterRule("One comma followed by a negative value in array()", "custom(none url(shader), id array(,-4))");
testInvalidFilterRule("One comma followed by a value in array()", "custom(none url(shader), id array(,4))");
testInvalidFilterRule("One negative value followed by a comma in array()", "custom(none url(shader), id array(-4,))");
testInvalidFilterRule("One value followed by a comma in array()", "custom(none url(shader), id array(4,))");
testInvalidFilterRule("A series of correct values followed by a comma in array()", "custom(none url(shader), id array(1, 2, 3, 4,))");
testInvalidFilterRule("A series of correct values followed by a letter in array()", "custom(none url(shader), id array(1, 2, 3, 4, x))");
testInvalidFilterRule("Two commas as separator between values in array()", "custom(none url(shader), id array(1, 2, , 4))");
testInvalidFilterRule("Leading comma in array()", "custom(none url(shader), id array(,1, 2, 3, 4))");
testInvalidFilterRule("Semicolon separated values in array()", "custom(none url(shader), id array(1; 2; 3; 4))");
testInvalidFilterRule("Space separated values in array()", "custom(none url(shader), id array(1 2 3 4))");
testInvalidFilterRule("Space separated values with comma terminator in array()", "custom(none url(shader), id array(1 2 3 4,))");
testInvalidFilterRule("Space separated values with leading comma in array()", "custom(none url(shader), id array(, 1 2 3 4))");
testInvalidFilterRule("NaN in array()", "custom(none url(shader), id array(NaN))");
testInvalidFilterRule("NaN and values in array()", "custom(none url(shader), id array(1, 2, NaN, 3))");
testInvalidFilterRule("Invalid unit value in array()", "custom(none url(shader), id array(1px))");
testInvalidFilterRule("Invalid unit value in array(), second", "custom(none url(shader), id array(none))");
testInvalidFilterRule("Invalid unit value in array(), third", "custom(none url(shader), id array(1deg))");
testInvalidFilterRule("Invalid unit value in array(), at the end of a partially valid value list", "custom(none url(shader), id array(1, 2, 3, 4px))");
