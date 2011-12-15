description("Test the parsing of the -webkit-filter property.");

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

testInvalidFilterRule("Too many parameters", "url(#a #b)");

testInvalidFilterRule("Length instead of number", "grayscale(10px)");
testInvalidFilterRule("Too many parameters", "grayscale(0.5 0.5)");
testInvalidFilterRule("Too many parameters and commas", "grayscale(0.5, 0.5)");
testInvalidFilterRule("Trailing comma", "grayscale(0.5,)");
testInvalidFilterRule("Negative parameter", "grayscale(-0.5)");
testInvalidFilterRule("Parameter out of bounds", "grayscale(1.5)");

testInvalidFilterRule("Too many parameters", "sepia(0.5 0.5 3.0)");
testInvalidFilterRule("Too many parameters and commas", "sepia(0.1, 0.1)");
testInvalidFilterRule("Trailing comma", "sepia(0.5,)");
testInvalidFilterRule("Negative parameter", "sepia(-0.01)");
testInvalidFilterRule("Parameter out of bounds", "sepia(10000)");

testInvalidFilterRule("Length instead of number", "saturate(10px)");
testInvalidFilterRule("Too many parameters", "saturate(0.5 0.5)");
testInvalidFilterRule("Too many parameters and commas", "saturate(0.5, 0.5)");
testInvalidFilterRule("Trailing comma", "saturate(0.5,)");
testInvalidFilterRule("Negative parameter", "saturate(-0.5)");

testInvalidFilterRule("Bare number", "hue-rotate(10)");
testInvalidFilterRule("Length", "hue-rotate(10px)");
testInvalidFilterRule("Too many parameters", "hue-rotate(10deg 4)");
testInvalidFilterRule("Too many parameters and commas", "hue-rotate(10deg, 5deg)");
testInvalidFilterRule("Trailing comma", "hue-rotate(10deg,)");

testInvalidFilterRule("Length instead of number", "invert(10px)");
testInvalidFilterRule("Too many parameters", "invert(0.5 0.5)");
testInvalidFilterRule("Too many parameters and commas", "invert(0.5, 0.5)");
testInvalidFilterRule("Trailing comma", "invert(0.5,)");
testInvalidFilterRule("Negative parameter", "invert(-0.5)");
testInvalidFilterRule("Parameter out of bounds", "invert(1.5)");

testInvalidFilterRule("Length instead of number", "opacity(10px)");
testInvalidFilterRule("Too many parameters", "opacity(0.5 0.5)");
testInvalidFilterRule("Too many parameters and commas", "opacity(0.5, 0.5)");
testInvalidFilterRule("Trailing comma", "opacity(0.5,)");
testInvalidFilterRule("Negative parameter", "opacity(-0.5)");
testInvalidFilterRule("Parameter out of bounds", "opacity(1.5)");

testInvalidFilterRule("Length instead of number", "gamma(10px 10 10)");
testInvalidFilterRule("Length instead of number", "gamma(10 10px 10)");
testInvalidFilterRule("Length instead of number", "gamma(10 10 10px)");
testInvalidFilterRule("Negative number", "gamma(-0.1)");
testInvalidFilterRule("Negative number", "gamma(1.0 -0.1)");
testInvalidFilterRule("Too many parameters", "gamma(1.0 1.0 1.0 1.0)");
testInvalidFilterRule("Commas", "gamma(1.0, 1.0, 1.0)");
testInvalidFilterRule("Trailing comma", "gamma(1.0, 1.0,)");

testInvalidFilterRule("Bare number", "blur(1)");
testInvalidFilterRule("Bare number", "blur(1px 1)");
testInvalidFilterRule("Negative number", "blur(-1px 1px)");
testInvalidFilterRule("Negative number", "blur(1px -1px)");
testInvalidFilterRule("Too many parameters", "blur(1px 1px 1px)");
testInvalidFilterRule("Too many parameters and commas", "blur(1em, 1em, 2em)");
testInvalidFilterRule("Commas", "blur(10px, 10px)");
testInvalidFilterRule("Trailing comma", "blur(1em,)");

testInvalidFilterRule("Missing length", "sharpen(1 1 1)");
testInvalidFilterRule("Wrong length type", "sharpen(1 1deg 1)");
testInvalidFilterRule("Too many parameters", "sharpen(1 1px 1 1)");
testInvalidFilterRule("Too many parameters and commas", "sharpen(1, 1px, 1, 1)");
testInvalidFilterRule("Commas", "sharpen(1, 10px, 1)");
testInvalidFilterRule("Trailing comma", "sharpen(1, 10px,)");

testInvalidFilterRule("No values", "drop-shadow()");
testInvalidFilterRule("Missing lengths", "drop-shadow(red)");
testInvalidFilterRule("Not enough lengths", "drop-shadow(red 1px)");
testInvalidFilterRule("Missing units", "drop-shadow(red 1 2 3)");
testInvalidFilterRule("Inset", "drop-shadow(red 1px 2px 3px inset)");
testInvalidFilterRule("Too many parameters", "drop-shadow(red 1px 2px 3px 4px)");
testInvalidFilterRule("Commas", "drop-shadow(red, 1px, 2px, 3px)");

successfullyParsed = true;
