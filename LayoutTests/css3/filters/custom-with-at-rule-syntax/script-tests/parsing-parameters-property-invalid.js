// Requires custom-filter-parsing-common.js.

description("Test parameters property parsing in the @-webkit-filter at-rule.");

// These have to be global for the test helpers to see them.
var filterAtRule, styleDeclaration;

function testInvalidParametersProperty(description, propertyValue)
{
    var parametersProperty = "parameters: " + propertyValue + ";";
    debug("\n" + description + "\n" + parametersProperty);

    stylesheet.insertRule("@-webkit-filter my-filter { " + parametersProperty + " }", 0);

    filterAtRule = stylesheet.cssRules.item(0);
    shouldBe("filterAtRule.type", "CSSRule.WEBKIT_FILTER_RULE");

    styleDeclaration = filterAtRule.style;
    shouldBe("styleDeclaration.length", "0");
    shouldBe("styleDeclaration.getPropertyValue('parameters')", "null");
}

heading("Number parameter tests.");
testInvalidParametersProperty("Too many parameter values.", "n 1 2 3 4 5");
testInvalidParametersProperty("Invalid parameter type.", "n 1.0 2.0 'text'");

testInvalidParametersProperty("No parameter definition.", "");
testInvalidParametersProperty("No parameter definition with comma.", ",");
testInvalidParametersProperty("No parameter definition before valid parameter defintion.", ", n 1");
testInvalidParametersProperty("No parameter value.", "n");
testInvalidParametersProperty("No parameter value with multiple parameters.", "n1, n2, n3");

heading("Transform parameter tests.");
testInvalidParametersProperty("One invalid transform function.", "t invalid-rotate(0deg)");
testInvalidParametersProperty("Multiple invalid transform functions.", "t invalid-rotate(0deg) invalid-perspective(0)");
testInvalidParametersProperty("Invalid transform function between valid ones.", "t rotate(0deg) invalid-rotate(0deg) perspective(0)");
testInvalidParametersProperty("Valid transform function between invalid ones.", "t invalid-rotate(0deg) perspective(0) invalid-translate(0)");
testInvalidParametersProperty("Valid transform function with trailing comma.", "t perspective(0),");
testInvalidParametersProperty("Invalid transform with trailing comma.", "t invalid-rotate(0deg),");
testInvalidParametersProperty("Valid transform with invalid characters", "t rotate(0deg) *.-,");

heading("Array parameter tests.");
testInvalidParametersProperty("Empty array.", "a array()");
testInvalidParametersProperty("One comma in array.", "a array(,)");
testInvalidParametersProperty("Multiple commas in array.", "a array(,,,)");
testInvalidParametersProperty("Multiple commas with a value in array.", "a array(,,1,)");
testInvalidParametersProperty("One comma followed by a negative value in array.", "a array(,-4)");
testInvalidParametersProperty("One comma followed by a value in array.", "a array(,4)");
testInvalidParametersProperty("One negative value followed by a comma in array.", "a array(-4,)");
testInvalidParametersProperty("One value followed by a comma in array.", "a array(4,)");
testInvalidParametersProperty("Valid values followed by a comma in array.", "a array(1, 2, 3, 4,)");
testInvalidParametersProperty("Valid values followed by a letter in array.", "a array(1, 2, 3, 4, x)");
testInvalidParametersProperty("Two commas as separator between values in array.", "a array(1, 2, , 4)");
testInvalidParametersProperty("Leading comma in array.", "a array(,1, 2, 3, 4)");
testInvalidParametersProperty("Semicolon separated values in array.", "a array(1; 2; 3; 4)");
testInvalidParametersProperty("Space separated values in array.", "a array(1 2 3 4)");
testInvalidParametersProperty("Space separated values with comma terminator in array.", "a array(1 2 3 4,)");
testInvalidParametersProperty("Space separated values with leading comma in array.", "a array(, 1 2 3 4)");
testInvalidParametersProperty("NaN in array.", "a array(NaN)");
testInvalidParametersProperty("NaN between valid values in array.", "a array(1, 2, NaN, 3)");
testInvalidParametersProperty("Invalid value 'none' in array.", "a array(none)");
testInvalidParametersProperty("Invalid value unit 'px' in array.", "a array(1px)");
testInvalidParametersProperty("Invalid value unit 'deg' in array.", "a array(1deg)");
testInvalidParametersProperty("Invalid value unit 'px' in array after valid values.", "a array(1, 2, 3, 4px)");
