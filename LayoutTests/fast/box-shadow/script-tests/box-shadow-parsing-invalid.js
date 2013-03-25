description("Test the parsing of box-shadow.");

// These have to be global for the test helpers to see them.
var stylesheet, cssRule, declaration;
var styleElement = document.createElement("style");
document.head.appendChild(styleElement);
stylesheet = styleElement.sheet;

function testInvalidFilterRule(description, rule)
{
    debug("");
    debug(description + " : " + rule);

    stylesheet.insertRule("body { box-shadow: " + rule + "; }", 0);
    cssRule = stylesheet.cssRules.item(0);

    shouldBe("cssRule.type", "1");

    declaration = cssRule.style;
    shouldBe("declaration.length", "0");
    shouldBe("declaration.getPropertyValue('box-shadow')", "null");
}

// FIXME A whole bunch of negative parsing tests are missing, see bug
// http://webkit.org/b/111498
testInvalidFilterRule("Negative blur radius value", "10px 10px -1px rgb(255, 0, 0)");
testInvalidFilterRule("Negative blur radius value, with a spread defined", "10px 10px -1px 10px rgb(255, 0, 0)");
testInvalidFilterRule("Negative blur radius value, with a negative spread defined", "10px 10px -1px -1px rgb(255, 0, 0)");

successfullyParsed = true;
