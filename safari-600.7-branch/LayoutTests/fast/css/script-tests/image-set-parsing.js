description("Test the parsing of the -webkit-image-set function.");

function jsWrapperClass(node)
{
    if (!node)
        return "[null]";
    var string = Object.prototype.toString.apply(node);
    return string.substr(8, string.length - 9);
}

function shouldBeType(expression, className, prototypeName, constructorName)
{
    if (!prototypeName)
        prototypeName = className + "Prototype";
    if (!constructorName)
        constructorName = className + "Constructor";
    shouldBe("jsWrapperClass(" + expression + ")", "'" + className + "'");
    shouldBe("jsWrapperClass(" + expression + ".__proto__)", "'" + prototypeName + "'");
    shouldBe("jsWrapperClass(" + expression + ".constructor)", "'" + constructorName + "'");
}

// These have to be global for the test helpers to see them.
var imageSetRule, subRule;

function testImageSetRule(description, property, rule, expectedLength, expectedTexts)
{
    debug("");
    debug(description + " : " + rule);

    var div = document.createElement("div");
    div.setAttribute("style", property + ": -webkit-image-set(" + rule + ")");
    document.body.appendChild(div);

    imageSetRule = div.style.getPropertyCSSValue(property);
    shouldBeType("imageSetRule", "CSSValueList");

    if (imageSetRule) {
        if (jsWrapperClass(imageSetRule[0]) == "CSSValueList") {
            // The content property returns a CSSValueList anyway, so to get to the 
            // imageSet CSS value list, we have to look at the first entry in the 
            // content value list.
            imageSetRule = imageSetRule[0];
        }
    }

    shouldBe("imageSetRule.length", "" + expectedLength); // shouldBe expects string arguments

    if (imageSetRule) {
        for (var i = 0; i < expectedLength; i++) {
            string = imageSetRule[i];
            if (i % 2 == 0) {
                subRule = string.cssText.split('#')[1];
                subRule = subRule.split(')')[0];
                shouldBe("subRule", "'" + expectedTexts[i] + "'");
            } else {
                subRule = string;
                shouldBe("subRule.cssText", "'" + expectedTexts[i] + "'");
            }
        }
    }

    document.body.removeChild(div);
}

testImageSetRule("Single value for background-image",
                "background-image",
                "url('#a') 1x", 2,
                ["a", "1"]);

testImageSetRule("Multiple values for background-image",
                "background-image",
                "url('#a') 1x, url('#b') 2x", 4,
                ["a", "1", "b", "2"]);

testImageSetRule("Multiple values for background-image, out of order",
                "background-image",
                "url('#c') 3x, url('#b') 2x, url('#a') 1x", 6,
                ["c", "3", "b", "2", "a", "1"]);

testImageSetRule("Single value for content",
                "content",
                "url('#a') 1x", 2,
                ["a", "1"]);

testImageSetRule("Multiple values for content",
                "content",
                "url('#a') 1x, url('#b') 2x", 4,
                ["a", "1", "b", "2"]);

testImageSetRule("Single value for border-image",
                "-webkit-border-image",
                "url('#a') 1x", 2,
                ["a", "1"]);

testImageSetRule("Multiple values for border-image",
                "-webkit-border-image",
                "url('#a') 1x, url('#b') 2x", 4,
                ["a", "1", "b", "2"]);

testImageSetRule("Single value for -webkit-mask-box-image",
                "-webkit-mask-box-image",
                "url('#a') 1x", 2,
                ["a", "1"]);

testImageSetRule("Multiple values for -webkit-mask-box-image",
                "-webkit-mask-box-image",
                "url('#a') 1x, url('#b') 2x", 4,
                ["a", "1", "b", "2"]);

successfullyParsed = true;
