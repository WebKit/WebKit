description("Test the parsing of the -webkit-mix-blend-mode property.");

var styleElement = document.createElement("style");
document.head.appendChild(styleElement);
var stylesheet = styleElement.sheet;
var cssRule;
var declaration;

function testblendmode(blendmode)
{
// add a -webkit-filter property to the start of the stylesheet
stylesheet.addRule("body", "-webkit-mix-blend-mode: " + blendmode, 0);

cssRule = stylesheet.cssRules.item(0);

shouldBe("cssRule.type", "1");

declaration = cssRule.style;
shouldBe("declaration.length", "1");
shouldBe("declaration.getPropertyValue('-webkit-mix-blend-mode')", "\'" + blendmode + "\'");
}

var blendmodes = ["normal", "multiply", "screen", "overlay", "darken", "lighten", "color-dodge", "color-burn", "hard-light", "soft-light", "difference", "exclusion", "hue", "saturation", "color", "luminosity"];

for(x in blendmodes)
   testblendmode(blendmodes[x]);

successfullyParsed = true;
