description("Test the parsing of the -webkit-image-set function.");

// These have to be global for the test helpers to see them.
var cssRule;

function testInvalidImageSet(description, property, rule)
{
    debug("");
    debug(description + " : " + rule);

    var div = document.createElement("div");
    div.setAttribute("style", property + ": -webkit-image-set(" + rule + ")");
    document.body.appendChild(div);

    cssRule = div.style.getPropertyCSSValue(property);
    shouldBe("cssRule", "null");

    document.body.removeChild(div);
}

testInvalidImageSet("Too many url parameters", "background-image", "url(#a #b)");
testInvalidImageSet("No x", "background-image", "url('#a') 1");
testInvalidImageSet("No comma", "background-image", "url('#a') 1x url('#b') 2x");
testInvalidImageSet("Too many scale factor parameters", "background-image", "url('#a') 1x 2x");
testInvalidImageSet("Scale factor is 0", "background-image", "url('#a') 0x");
testInvalidImageSet("No url function", "background-image", "'#a' 1x");

successfullyParsed = true;
