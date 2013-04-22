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

heading("Color parameter tests.");
testInvalidFilterRule("No rgb color values.", "custom(my-filter, c rgb(,,))");
testInvalidFilterRule("No hsl color values.", "custom(my-filter, c hsl(,,))");
testInvalidFilterRule("Hex with 8 characters.", "custom(my-filter, c #FF0000FF)");
testInvalidFilterRule("Hex with 2 character.", "custom(my-filter, c #FF)");
testInvalidFilterRule("Rgba with 3 values and 2 commas.", "custom(my-filter, c rgba(255, 0, 0))");

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


heading("Mat2 parameter tests.");
testInvalidFilterRule("Empty mat2.", "custom(my-filter, a mat2())");
testInvalidFilterRule("Too view arguments.", "custom(my-filter, a mat2(0, 0, 0))");
testInvalidFilterRule("No arguments but commas.", "custom(my-filter, a mat2(,,,))");
testInvalidFilterRule("Ending commas.", "custom(my-filter, a mat2(0, 0,,))");
testInvalidFilterRule("Idents in mat2 function.", "custom(my-filter, a mat2(0, 0, a, b))");
testInvalidFilterRule("Too many arguments.", "custom(my-filter, a mat2(0, 0, 0, 0, 1))");
testInvalidFilterRule("No commas.", "custom(my-filter, a mat2(1 0 0 1))");
testInvalidFilterRule("Some commas.", "custom(my-filter, a mat2(1, 0, 0 1))");
testInvalidFilterRule("Leading commas.", "custom(my-filter, a mat2(, 0, 0, 1))");
testInvalidFilterRule("No length units.", "custom(my-filter, a mat2(1px, 0px, 0px, 1px))");
testInvalidFilterRule("No degree units.", "custom(my-filter, a mat2(1deg, 0deg, 0deg, 1deg))");
testInvalidFilterRule("NaN in mat2.", "custom(my-filter, a mat2(1, 0, 0, NaN))");

heading("Mat3 parameter tests.");
testInvalidFilterRule("Empty mat3.", "custom(my-filter, a mat3())");
testInvalidFilterRule("Too view arguments.", "custom(my-filter, a mat3(0, 0, 0, 0))");
testInvalidFilterRule("No arguments but commas.", "custom(my-filter, a mat3(,,,,,,,,,,,,,,,))");
testInvalidFilterRule("Ending commas.", "custom(my-filter, a mat3(1, 0, 0, 0, 1, 0, 0,,))");
testInvalidFilterRule("Idents in mat3 function.", "custom(my-filter, a mat3(1, 0, 0, 0, 1, 0, 0, a, b))");
testInvalidFilterRule("Too many arguments.", "custom(my-filter, a mat3(1, 0, 0, 0, 1, 0, 0, 0, 1, 0))");
testInvalidFilterRule("No commas.", "custom(my-filter, a mat3(1 0 0 0 1 0 0 0 1))");
testInvalidFilterRule("Some commas.", "custom(my-filter, a mat3(1, 0, 0, 0, 1, 0 0 0 1))");
testInvalidFilterRule("Leading commas.", "custom(my-filter, a mat3(, 0, 0, 0, 1, 0, 0, 0, 1))");
testInvalidFilterRule("No length units.", "custom(my-filter, a mat3(1px, 0, 0, 0, 1px, 0, 0, 0, 1px))");
testInvalidFilterRule("No degree units.", "custom(my-filter, a mat3(1deg, 0, 0, 0, 1deg, 0, 0, 0, 1deg))");
testInvalidFilterRule("NaN in mat3.", "custom(my-filter, a mat2(NaN, 0, 0, 0, 1, 0, 0, 0, 1))");

heading("Mat4 parameter tests.");
testInvalidFilterRule("Empty mat4.", "custom(my-filter, a mat4())");
testInvalidFilterRule("Too view arguments.", "custom(my-filter, a mat4(1, 0, 0, 0, 1, 0, 0, 0, 1))");
testInvalidFilterRule("No arguments but commas.", "custom(my-filter, a mat4(,,,,,,,,,,,,,,,))");
testInvalidFilterRule("Ending commas.", "custom(my-filter, a mat4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0,,))");
testInvalidFilterRule("Idents in mat4 function.", "custom(my-filter, a mat4(1, 0, 0, 0, 1, 0, 0, a, b))");
testInvalidFilterRule("Too many arguments.", "custom(my-filter, a mat4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0))");
testInvalidFilterRule("No commas.", "custom(my-filter, a mat4(1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1))");
testInvalidFilterRule("Some commas.", "custom(my-filter, a mat4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0 0 0 0 1))");
testInvalidFilterRule("Leading commas.", "custom(my-filter, a mat4(, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1))");
testInvalidFilterRule("No length units.", "custom(my-filter, a mat4(1px, 0, 0, 0, 0, 1px, 0, 0, 0, 0, 1px, 0, 0, 0, 0, 1px))");
testInvalidFilterRule("No degree units.", "custom(my-filter, a mat4(1deg, 0, 0, 0, 0, 1deg, 0, 0, 0, 0, 1deg, 0, 0, 0, 0, 1deg))");
testInvalidFilterRule("NaN in mat4.", "custom(my-filter, a mat4(NaN, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1))");


heading("Mixing parameter types.");
testInvalidFilterRule("Number parameter with hex color.", "custom(my-filter, n1 1 2 #FF0000)");
testInvalidFilterRule("Number parameter with color keyword.", "custom(my-filter, n1 1 2 red)");
testInvalidFilterRule("Number parameter with rgb color.", "custom(my-filter, n1 1 2 rgb(255, 0, 0))");
testInvalidFilterRule("Color with number parameter.", "custom(my-filter, a rgb(255, 0, 0) 1)");
testInvalidFilterRule("Color in array.", "custom(my-filter, a array(0, rgb(255, 0, 0)))");
testInvalidFilterRule("Color and array.", "custom(my-filter, a array(0, 0) rgb(255, 0, 0))");
testInvalidFilterRule("Color with transform values.", "custom(my-filter, a rotate(45deg) rgb(255, 0, 0))");
testInvalidFilterRule("Color with transform values.", "custom(my-filter, a rgb(255, 0, 0) rotate(45deg))");
testInvalidFilterRule("Color with mat2.", "custom(my-filter, a rgb(255, 0, 0) mat2(0, 0, 0, 0))");
testInvalidFilterRule("mat2 with mat3.", "custom(my-filter, a mat2(1, 0, 0, 1) mat3(0, 0, 0, 0, 0, 0, 0, 0, 0))");
