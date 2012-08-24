description("Test the setting of the -webkit-image-set function.");

function testComputedStyle(property, rule) 
{
    var div = document.createElement("div");
    document.body.appendChild(div);
    div.setAttribute("style", property + ": -webkit-image-set(" + rule + ")");
    var computedValue = div.style.background;
    document.body.removeChild(div);
    return computedValue;
}

function testImageSetRule(description, property, rule, expected)
{
    debug("");
    debug(description + " : " + rule);

    shouldBeEqualToString('testComputedStyle("' + property + '", "' + rule + '")', expected);
}

testImageSetRule("Single value for background-image",
                "background-image",
                "url('http://www.webkit.org/a') 1x", 
                "-webkit-image-set(url(http://www.webkit.org/a), 1)");

testImageSetRule("Multiple values for background-image",
                "background-image",
                "url('http://www.webkit.org/a') 1x, url('http://www.webkit.org/b') 2x", 
                "-webkit-image-set(url(http://www.webkit.org/a), 1, url(http://www.webkit.org/b), 2)");

testImageSetRule("Multiple values for background-image, out of order",
                "background-image",
                "url('http://www.webkit.org/c') 3x, url('http://www.webkit.org/b') 2x, url('http://www.webkit.org/a') 1x", 
                "-webkit-image-set(url(http://www.webkit.org/c), 3, url(http://www.webkit.org/b), 2, url(http://www.webkit.org/a), 1)");

successfullyParsed = true;
