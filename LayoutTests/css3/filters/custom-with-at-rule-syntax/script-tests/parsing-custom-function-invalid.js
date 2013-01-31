description("Test the parsing of custom() function of the -webkit-filter property.");

// These have to be global for the test helpers to see them.
var styleRule, declaration;

function testInvalidFilterRule(description, rule)
{
    debug("\n" + description + "\n" + rule);

    stylesheet.insertRule("body { -webkit-filter: " + rule + "; }", 0);

    styleRule = stylesheet.cssRules.item(0);
    shouldBe("styleRule.type", "CSSRule.STYLE_RULE");

    declaration = styleRule.style;
    shouldBe("declaration.length", "0");
    shouldBe("declaration.getPropertyValue('-webkit-filter')", "null");
}

heading("Custom function tests.");
testInvalidFilterRule("Empty custom function.", "custom()");
testInvalidFilterRule("One comma in custom function.", "custom(,)");
testInvalidFilterRule("Multiple commas in custom function.", "custom(,,)");

heading("Filter name tests.");
testInvalidFilterRule("No filter name with parameter.", "custom(n 1)");
testInvalidFilterRule("Too many filter names.", "custom(my-filter-1 my-filter-2)");
testInvalidFilterRule("Filter name as string.", "custom('my-filter-1')");
testInvalidFilterRule("Filter name as number.", "custom(1)");
testInvalidFilterRule("Space between filter name and parameter.", "custom(my-filter n 1)");

heading("Number parameter tests.");
testInvalidFilterRule("Too many parameter values.", "custom(my-filter, n 1 2 3 4 5)");
testInvalidFilterRule("Invalid parameter type.", "custom(my-filter, n 1.0 2.0 'text')");

testInvalidFilterRule("No parameter definition after comma.", "custom(my-filter,)");
testInvalidFilterRule("No parameter definition with two commas.", "custom(my-filter,,)");
testInvalidFilterRule("No parameter definition before valid parameter defintion.", "custom(my-filter, , n 1)");
testInvalidFilterRule("No parameter value.", "custom(my-filter, n)");
testInvalidFilterRule("No parameter value with multiple parameters.", "custom(my-filter, n1, n2, n3)");

heading("Transform parameter tests.");
testInvalidFilterRule("One invalid transform function.", "custom(my-filter, t invalid-rotate(0deg))");
testInvalidFilterRule("Multiple invalid transform functions.", "custom(my-filter, t invalid-rotate(0deg) invalid-perspective(0))");
testInvalidFilterRule("Invalid transform function between valid ones.", "custom(my-filter, t rotate(0deg) invalid-rotate(0deg) perspective(0))");
testInvalidFilterRule("Valid transform function between invalid ones.", "custom(my-filter, t invalid-rotate(0deg) perspective(0) invalid-translate(0))");
testInvalidFilterRule("Valid transform function without leading comma.", "custom(my-filter t perspective(0))");
testInvalidFilterRule("Valid transform function with trailing comma.", "custom(my-filter, t perspective(0),)");
testInvalidFilterRule("Valid transform function with trailing comma and without leading comma.", "custom(my-filter t perspective(0),)");
testInvalidFilterRule("Invalid transform with trailing comma.", "custom(my-filter, t invalid-rotate(0deg),)");
testInvalidFilterRule("Invalid transform without leading comma.", "custom(my-filter t invalid_rotate(0deg))");
testInvalidFilterRule("Valid transform with invalid characters", "custom(my-filter,t rotate(0deg) *.-,)");

heading("Array parameter tests.");
testInvalidFilterRule("Empty array.", "custom(my-filter, a array())");
testInvalidFilterRule("One comma in array.", "custom(my-filter, a array(,))");
testInvalidFilterRule("Multiple commas in array.", "custom(my-filter, a array(,,,))");
testInvalidFilterRule("Multiple commas with a value in array.", "custom(my-filter, a array(,,1,))");
testInvalidFilterRule("One comma followed by a negative value in array.", "custom(my-filter, a array(,-4))");
testInvalidFilterRule("One comma followed by a value in array.", "custom(my-filter, a array(,4))");
testInvalidFilterRule("One negative value followed by a comma in array.", "custom(my-filter, a array(-4,))");
testInvalidFilterRule("One value followed by a comma in array.", "custom(my-filter, a array(4,))");
testInvalidFilterRule("Valid values followed by a comma in array.", "custom(my-filter, a array(1, 2, 3, 4,))");
testInvalidFilterRule("Valid values followed by a letter in array.", "custom(my-filter, a array(1, 2, 3, 4, x))");
testInvalidFilterRule("Two commas as separator between values in array.", "custom(my-filter, a array(1, 2, , 4))");
testInvalidFilterRule("Leading comma in array.", "custom(my-filter, a array(,1, 2, 3, 4))");
testInvalidFilterRule("Semicolon separated values in array.", "custom(my-filter, a array(1; 2; 3; 4))");
testInvalidFilterRule("Space separated values in array.", "custom(my-filter, a array(1 2 3 4))");
testInvalidFilterRule("Space separated values with comma terminator in array.", "custom(my-filter, a array(1 2 3 4,))");
testInvalidFilterRule("Space separated values with leading comma in array.", "custom(my-filter, a array(, 1 2 3 4))");
testInvalidFilterRule("NaN in array.", "custom(my-filter, a array(NaN))");
testInvalidFilterRule("NaN between valid values in array.", "custom(my-filter, a array(1, 2, NaN, 3))");
testInvalidFilterRule("Invalid value 'none' in array.", "custom(my-filter, a array(none))");
testInvalidFilterRule("Invalid value unit 'px' in array.", "custom(my-filter, a array(1px))");
testInvalidFilterRule("Invalid value unit 'deg' in array.", "custom(my-filter, a array(1deg))");
testInvalidFilterRule("Invalid value unit 'px' in array after valid values.", "custom(my-filter, a array(1, 2, 3, 4px))");
