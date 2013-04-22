// Requires custom-filter-parsing-common.js.

description("Test parameters property parsing in the @-webkit-filter at-rule.");

// These have to be global for the test helpers to see them.
var filterAtRule, styleDeclaration, parametersPropertyValue, subValue;

function testParametersProperty(description, propertyValue, expectedValue, expectedTexts)
{
    var parametersProperty = "parameters: " + propertyValue + ";"
    debug("\n" + description + "\n" + parametersProperty);

    stylesheet.insertRule("@-webkit-filter my-filter { " + parametersProperty + " }", 0);
    filterAtRule = stylesheet.cssRules.item(0);
    shouldBe("filterAtRule.type", "CSSRule.WEBKIT_FILTER_RULE");

    styleDeclaration = filterAtRule.style;
    shouldBe("styleDeclaration.length", "1");
    shouldBe("removeBaseURL(styleDeclaration.getPropertyValue('parameters'))", "\"" + expectedValue + "\"");

    parametersPropertyValue = styleDeclaration.getPropertyCSSValue('parameters');
    shouldHaveConstructor("parametersPropertyValue", "CSSValueList");

    shouldBe("parametersPropertyValue.length", expectedTexts.length.toString()); // shouldBe expects string arguments
  
    if (parametersPropertyValue) {
        for (var i = 0; i < expectedTexts.length; i++) {
            subValue = parametersPropertyValue[i];
            shouldBe("subValue.cssValueType", "CSSValue.CSS_VALUE_LIST");
            shouldBe("removeBaseURL(subValue.cssText)", "\"" + expectedTexts[i] + "\"");
        }
    }
}

heading("Number parameter tests.")
testParametersProperty("Integer parameters.",
    "n1 1, n2 2 3, n3 3 4 5, n4 4 5 6 7",
    "n1 1, n2 2 3, n3 3 4 5, n4 4 5 6 7",
    ["n1 1","n2 2 3", "n3 3 4 5", "n4 4 5 6 7"]);
testParametersProperty("Float parameters.",
    "n1 1.1, n2 2.2 3.3, n3 3.1 4.1 5.1, n4 4.1 5.2 6.3 7.4",
    "n1 1.1, n2 2.2 3.3, n3 3.1 4.1 5.1, n4 4.1 5.2 6.3 7.4",
    ["n1 1.1", "n2 2.2 3.3", "n3 3.1 4.1 5.1", "n4 4.1 5.2 6.3 7.4"]);
testParametersProperty("Parameter name same as CSS keyword.",
    "background 0 1 0 1",
    "background 0 1 0 1",
    ["background 0 1 0 1"]);

heading("Transform parameter tests.")
testParametersProperty("Transform parameter with one transform function.",
    "t rotate(0deg)",
    "t rotate(0deg)",
    ["t rotate(0deg)"]);
testParametersProperty("Transform parameter with multiple transform functions.",
    "t rotate(0deg) perspective(0) scale(0, 0) translate(0px, 0px)",
    "t rotate(0deg) perspective(0) scale(0, 0) translate(0px, 0px)",
    ["t rotate(0deg) perspective(0) scale(0, 0) translate(0px, 0px)"]);
testParametersProperty("Mulitple transform parameters.",
    "t1 rotate(0deg), t2 rotate(0deg)",
    "t1 rotate(0deg), t2 rotate(0deg)",
    ["t1 rotate(0deg)", "t2 rotate(0deg)"]);

heading("Array parameter tests.");
testParametersProperty("Array parameter with name 'array'.",
    "array array(1)",
    "array array(1)",
    ["array array(1)"]);
testParametersProperty("Array parameter with one positive integer.",
    "a array(1)",
    "a array(1)",
    ["a array(1)"]);
testParametersProperty("Array parameter with one negative float.",
    "a array(-1.01)",
    "a array(-1.01)",
    ["a array(-1.01)"]);
testParametersProperty("Array parameter with multiple positive integers.",
    "a array(1, 2, 3, 4, 5)",
    "a array(1, 2, 3, 4, 5)",
    ["a array(1, 2, 3, 4, 5)"]);
testParametersProperty("Array parameter with multiple signed floats.",
    "a array(1, -2.2, 3.14, 0.4, 5)",
    "a array(1, -2.2, 3.14, 0.4, 5)",
    ["a array(1, -2.2, 3.14, 0.4, 5)"]);
testParametersProperty("Multiple array parameters.",
    "a1 array(1, -2.2, 3.14, 0.4, 5), a2 array(1, 2, 3)",
    "a1 array(1, -2.2, 3.14, 0.4, 5), a2 array(1, 2, 3)",
    ["a1 array(1, -2.2, 3.14, 0.4, 5)", "a2 array(1, 2, 3)"]);

heading("Color parameter tests.");
testParametersProperty("Hex color.",
    "c #00FF00",
    "c rgb(0, 255, 0)",
    ["c rgb(0, 255, 0)"]);
testParametersProperty("Color keyword.",
    "c green",
    "c rgb(0, 128, 0)",
    ["c rgb(0, 128, 0)"]);
testParametersProperty("Color rgb function.",
    "c rgb(0, 128, 0)",
    "c rgb(0, 128, 0)",
    ["c rgb(0, 128, 0)"]);
testParametersProperty("Color hsl function.",
    "c hsl(120, 100%, 50%)",
    "c rgb(0, 255, 0)",
    ["c rgb(0, 255, 0)"]);
testParametersProperty("Color rgba function.",
    "c rgba(0, 255, 0, 0.2)",
    "c rgba(0, 255, 0, 0.2)",
    ["c rgba(0, 255, 0, 0.2)"]);
testParametersProperty("Color hsla function.",
    "c hsla(120, 100%, 50%, 0.2)",
    "c rgba(0, 255, 0, 0.2)",
    ["c rgba(0, 255, 0, 0.2)"]);

heading("Mat2 parameter tests.");
testParametersProperty("Mat2 parameter.",
    "m mat2(1, 0, 0, 1)",
    "m mat2(1, 0, 0, 1)",
    ["m mat2(1, 0, 0, 1)"]);
testParametersProperty("Mat2 parameter with negative values.",
    "m mat2(-1, -1, -1, -1)",
    "m mat2(-1, -1, -1, -1)",
    ["m mat2(-1, -1, -1, -1)"]);
testParametersProperty("Mat2 parameter with negative and positive values.",
    "m mat2(-1, 1, 1, -1)",
    "m mat2(-1, 1, 1, -1)",
    ["m mat2(-1, 1, 1, -1)"]);
testParametersProperty("Mat2 parameter with multiple signed floats.",
    "m mat2(1, -2.2, 3.14, 0.4)",
    "m mat2(1, -2.2, 3.14, 0.4)",
    ["m mat2(1, -2.2, 3.14, 0.4)"]);

heading("Mat3 parameter tests.");
testParametersProperty("Mat3 parameter.",
    "m mat3(1, 0, 0, 0, 1, 0, 0, 0, 1)",
    "m mat3(1, 0, 0, 0, 1, 0, 0, 0, 1)",
    ["m mat3(1, 0, 0, 0, 1, 0, 0, 0, 1)"]);
testParametersProperty("Mat3 parameter with negative values.",
    "m mat3(-1, -1, -1, -1, -1, -1, -1, -1, -1)",
    "m mat3(-1, -1, -1, -1, -1, -1, -1, -1, -1)",
    ["m mat3(-1, -1, -1, -1, -1, -1, -1, -1, -1)"]);
testParametersProperty("Mat3 parameter with negative and positive values.",
    "m mat3(-1, 1, 1, -1, -1, 1, 1, -1, 1)",
    "m mat3(-1, 1, 1, -1, -1, 1, 1, -1, 1)",
    ["m mat3(-1, 1, 1, -1, -1, 1, 1, -1, 1)"]);
testParametersProperty("Mat3 parameter with multiple signed floats.",
    "m mat3(1, -2.2, 3.14, 0.4, 1, -2.2, 3.14, 0.4, 1)",
    "m mat3(1, -2.2, 3.14, 0.4, 1, -2.2, 3.14, 0.4, 1)",
    ["m mat3(1, -2.2, 3.14, 0.4, 1, -2.2, 3.14, 0.4, 1)"]);

heading("Mat4 parameter tests.");
testParametersProperty("Mat4 parameter.",
    "m mat4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1)",
    "m mat4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1)",
    ["m mat4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1)"]);
testParametersProperty("Mat4 parameter with negative values.",
    "m mat4(-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1)",
    "m mat4(-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1)",
    ["m mat4(-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1)"]);
testParametersProperty("Mat4 parameter with negative and positive values.",
    "m mat4(-1, 1, 1, -1, -1, 1, 1, -1, -1, 1, 1, -1, -1, 1, 1, -1)",
    "m mat4(-1, 1, 1, -1, -1, 1, 1, -1, -1, 1, 1, -1, -1, 1, 1, -1)",
    ["m mat4(-1, 1, 1, -1, -1, 1, 1, -1, -1, 1, 1, -1, -1, 1, 1, -1)"]);
testParametersProperty("Mat4 parameter with multiple signed floats.",
    "m mat4(1, -2.2, 3.14, 0.4, 1, -2.2, 3.14, 0.4, 1, -2.2, 3.14, 0.4, 1, -2.2, 3.14, 0.4)",
    "m mat4(1, -2.2, 3.14, 0.4, 1, -2.2, 3.14, 0.4, 1, -2.2, 3.14, 0.4, 1, -2.2, 3.14, 0.4)",
    ["m mat4(1, -2.2, 3.14, 0.4, 1, -2.2, 3.14, 0.4, 1, -2.2, 3.14, 0.4, 1, -2.2, 3.14, 0.4)"]);

heading("Combined parameter tests.");
testParametersProperty("Number parameter, color parameter, transform parameter, matrix parameters and array parameter.",
    "n 1, c rgb(0, 128, 0), t rotate(0deg), m1 mat2(1, 0, 0, 1), m2 mat3(1, 0, 0, 0, 1, 0, 0, 0, 1), m3 mat4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1), a array(1)",
    "n 1, c rgb(0, 128, 0), t rotate(0deg), m1 mat2(1, 0, 0, 1), m2 mat3(1, 0, 0, 0, 1, 0, 0, 0, 1), m3 mat4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1), a array(1)",
    ["n 1", "c rgb(0, 128, 0)", "t rotate(0deg)", "m1 mat2(1, 0, 0, 1)", "m2 mat3(1, 0, 0, 0, 1, 0, 0, 0, 1)", "m3 mat4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1)", "a array(1)"]);