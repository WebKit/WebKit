description("Test the parsing of the -webkit-filter property.");

var styleElement = document.createElement("style");
document.head.appendChild(styleElement);
var stylesheet = styleElement.sheet;

// add a -webkit-filter property to the start of the stylesheet
stylesheet.addRule("body", "-webkit-filter: url(#a) url(#b)", 0);

var cssRule = stylesheet.cssRules.item(0);

shouldBe("cssRule.type", "1");

var declaration = cssRule.style;
shouldBe("declaration.length", "1");
shouldBe("declaration.getPropertyValue('-webkit-filter')", "'url(#a) url(#b)'");

successfullyParsed = true;
