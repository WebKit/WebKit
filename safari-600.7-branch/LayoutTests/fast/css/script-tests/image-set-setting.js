description("Test the setting of the -webkit-image-set function.");

function testComputedStyle(property, fullRule)
{
    var div = document.createElement("div");
    document.body.appendChild(div);
    div.setAttribute("style", property + ": " + fullRule);
    var computedValue = div.style.background;
    document.body.removeChild(div);
    return computedValue;
}

function testImageSetRule(description, property, rule, expected)
{
    debug("");
    debug(description + " : " + rule);

    var fullRule = "-webkit-image-set(" + rule + ")";
    shouldBeEqualToString('testComputedStyle("' + property + '", "' + fullRule + '")', fullRule);
}

testImageSetRule("Single value for background-image",
                "background-image",
                "url(http://www.webkit.org/a) 1x");

testImageSetRule("Multiple values for background-image",
                "background-image",
                "url(http://www.webkit.org/a) 1x, url(http://www.webkit.org/b) 2x");

testImageSetRule("Multiple values for background-image, out of order",
                "background-image",
                "url(http://www.webkit.org/c) 3x, url(http://www.webkit.org/b) 2x, url(http://www.webkit.org/a) 1x");

testImageSetRule("Duplicate values for background-image",
                "background-image",
                "url(http://www.webkit.org/c) 1x, url(http://www.webkit.org/b) 2x, url(http://www.webkit.org/a) 1x");

testImageSetRule("Fractional values for background-image",
                "background-image",
                "url(http://www.webkit.org/c) 0.2x, url(http://www.webkit.org/b) 2.3x, url(http://www.webkit.org/a) 12.3456x");

// FIXME: We should also be testing the behavior of negative values somewhere, but it's currently
// broken.  http://wkb.ug/100132

successfullyParsed = true;
