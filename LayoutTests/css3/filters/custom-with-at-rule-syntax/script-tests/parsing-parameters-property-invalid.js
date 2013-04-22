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

heading("Color parameter tests.");
testInvalidParametersProperty("No rgb color values.", "c rgb(,,)");
testInvalidParametersProperty("No hsl color values.", "c hsl(,,)");
testInvalidParametersProperty("Hex with 8 characters.", "c #FF0000FF");
testInvalidParametersProperty("Hex with 2 character.", "c #FF");
testInvalidParametersProperty("Rgba with 3 values and 2 commas.", "c rgba(255, 0, 0)");

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

heading("Mat2 parameter tests.");
testInvalidParametersProperty("Empty mat2.", "a mat2()");
testInvalidParametersProperty("Too view arguments.", "a mat2(0, 0, 0)");
testInvalidParametersProperty("No arguments but commas.", "a mat2(,,,)");
testInvalidParametersProperty("Ending commas.", "a mat2(0, 0,,)");
testInvalidParametersProperty("Idents in mat2 function.", "a mat2(0, 0, a, b)");
testInvalidParametersProperty("Too many arguments.", "a mat2(0, 0, 0, 0, 1)");
testInvalidParametersProperty("No commas.", "a mat2(1 0 0 1)");
testInvalidParametersProperty("Some commas.", "a mat2(1, 0, 0 1)");
testInvalidParametersProperty("Leading commas.", "a mat2(, 0, 0, 1)");
testInvalidParametersProperty("No length units.", "a mat2(1px, 0px, 0px, 1px)");
testInvalidParametersProperty("No degree units.", "a mat2(1deg, 0deg, 0deg, 1deg)");
testInvalidParametersProperty("NaN in mat2.", "a mat2(1, 0, 0, NaN)");

heading("Mat3 parameter tests.");
testInvalidParametersProperty("Empty mat3.", "a mat3()");
testInvalidParametersProperty("Too view arguments.", "a mat3(0, 0, 0, 0)");
testInvalidParametersProperty("No arguments but commas.", "a mat3(,,,,,,,,,,,,,,,)");
testInvalidParametersProperty("Ending commas.", "a mat3(1, 0, 0, 0, 1, 0, 0,,)");
testInvalidParametersProperty("Idents in mat3 function.", "a mat3(1, 0, 0, 0, 1, 0, 0, a, b)");
testInvalidParametersProperty("Too many arguments.", "a mat3(1, 0, 0, 0, 1, 0, 0, 0, 1, 0)");
testInvalidParametersProperty("No commas.", "a mat3(1 0 0 0 1 0 0 0 1)");
testInvalidParametersProperty("Some commas.", "a mat3(1, 0, 0, 0, 1, 0 0 0 1)");
testInvalidParametersProperty("Leading commas.", "a mat3(, 0, 0, 0, 1, 0, 0, 0, 1)");
testInvalidParametersProperty("No length units.", "a mat3(1px, 0, 0, 0, 1px, 0, 0, 0, 1px)");
testInvalidParametersProperty("No degree units.", "a mat3(1deg, 0, 0, 0, 1deg, 0, 0, 0, 1deg)");
testInvalidParametersProperty("NaN in mat3.", "a mat2(NaN, 0, 0, 0, 1, 0, 0, 0, 1)");

heading("Mat4 parameter tests.");
testInvalidParametersProperty("Empty mat4.", "a mat4()");
testInvalidParametersProperty("Too view arguments.", "a mat4(1, 0, 0, 0, 1, 0, 0, 0, 1)");
testInvalidParametersProperty("No arguments but commas.", "a mat4(,,,,,,,,,,,,,,,)");
testInvalidParametersProperty("Ending commas.", "a mat4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0,,)");
testInvalidParametersProperty("Idents in mat4 function.", "a mat4(1, 0, 0, 0, 1, 0, 0, a, b)");
testInvalidParametersProperty("Too many arguments.", "a mat4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0)");
testInvalidParametersProperty("No commas.", "a mat4(1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1)");
testInvalidParametersProperty("Some commas.", "a mat4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0 0 0 0 1)");
testInvalidParametersProperty("Leading commas.", "a mat4(, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1)");
testInvalidParametersProperty("No length units.", "a mat4(1px, 0, 0, 0, 0, 1px, 0, 0, 0, 0, 1px, 0, 0, 0, 0, 1px)");
testInvalidParametersProperty("No degree units.", "a mat4(1deg, 0, 0, 0, 0, 1deg, 0, 0, 0, 0, 1deg, 0, 0, 0, 0, 1deg)");
testInvalidParametersProperty("NaN in mat4.", "a mat4(NaN, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1)");

heading("Mixing parameter types.");
testInvalidParametersProperty("Number parameter with hex color.", "n1 1 2 #FF0000");
testInvalidParametersProperty("Number parameter with color keyword.", "n1 1 2 red");
testInvalidParametersProperty("Number parameter with rgb color.", "n1 1 2 rgb(255, 0, 0)");
testInvalidParametersProperty("Color with number parameter.", "a rgb(255, 0, 0) 1");
testInvalidParametersProperty("Color in array.", "a array(0, rgb(255, 0, 0))");
testInvalidParametersProperty("Color and array.", "a array(0, 0) rgb(255, 0, 0)");
testInvalidParametersProperty("Color with transform values.", "a rotate(45deg) rgb(255, 0, 0)");
testInvalidParametersProperty("Color with transform values.", "a rgb(255, 0, 0) rotate(45deg)");
testInvalidParametersProperty("Color after array values.", "a array(0) rgb(255, 0, 0)");
testInvalidParametersProperty("Color with mat2.", "a rgb(255, 0, 0) mat2(0, 0, 0, 0)");
testInvalidParametersProperty("mat2 with mat3.", "a mat2(1, 0, 0, 1) mat3(0, 0, 0, 0, 0, 0, 0, 0, 0)");

