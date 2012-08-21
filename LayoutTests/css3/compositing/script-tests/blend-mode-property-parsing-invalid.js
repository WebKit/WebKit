description("Test the parsing of the -webkit-blend-mode property.");

// These have to be global for the test helpers to see them.
var stylesheet, cssRule, declaration;
var styleElement = document.createElement("style");
document.head.appendChild(styleElement);
stylesheet = styleElement.sheet;
stylesheet.insertRule("body { -webkit-blend-mode: multiply; }", 0);

function testInvalidFilterRule(description, rule)
{
    debug("");
    debug(description + " : " + rule);

    stylesheet.insertRule("body { -webkit-blend-mode: " + rule + "; }", 0);
    cssRule = stylesheet.cssRules.item(0);
  
    shouldBe("cssRule.type", "1");

    declaration = cssRule.style;
    shouldBe("declaration.length", "0");
    shouldBe("declaration.getPropertyValue('-webkit-blend-mode')", "null");
}

testInvalidFilterRule("Too many parameters", "overlay overlay");
testInvalidFilterRule("Wrong type", "\"5px\"");
testInvalidFilterRule("Trailing comma", "overlay,");


successfullyParsed = true;
