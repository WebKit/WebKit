// Requires custom-filter-parsing-common.js.

description("Test src property parsing in the @-webkit-filter at-rule.");

// These have to be global for the test helpers to see them.
var filterAtRule, styleDeclaration, srcPropertyValue, subValue;

function testSrcProperty(description, propertyValue, expectedValue, expectedTexts)
{
    var srcProperty = "src: " + propertyValue + ";"
    debug("\n" + description + "\n" + srcProperty);

    stylesheet.insertRule("@-webkit-filter my-filter { " + srcProperty + " }", 0);
    filterAtRule = stylesheet.cssRules.item(0);
    shouldBe("filterAtRule.type", "CSSRule.WEBKIT_FILTER_RULE");

    styleDeclaration = filterAtRule.style;
    shouldBe("styleDeclaration.length", "1");
    shouldBe("removeBaseURL(styleDeclaration.getPropertyValue('src'))", "\"" + expectedValue + "\"");

    srcPropertyValue = styleDeclaration.getPropertyCSSValue('src');
    shouldHaveConstructor("srcPropertyValue", "CSSValueList");

    shouldBe("srcPropertyValue.length", expectedTexts.length.toString()); // shouldBe expects string arguments
  
    if (srcPropertyValue) {
        for (var i = 0; i < expectedTexts.length; i++) {
            subValue = srcPropertyValue[i];
            shouldHaveConstructor("subValue", "CSSValue");
            shouldBe("subValue.cssValueType", "CSSValue.CSS_CUSTOM");
            shouldBe("removeBaseURL(subValue.cssText)", "\"" + expectedTexts[i] + "\"");
        }
    }
}

heading("URLs without format tests.");
testSrcProperty("One URL without format.",
    "url(shader.vs)",
    "url(shader.vs)",
    ["url(shader.vs)"]);

testSrcProperty("Two URLs without format.",
    "url(shader.vs), url(shader.fs)",
    "url(shader.vs), url(shader.fs)",
    ["url(shader.vs)",
    "url(shader.fs)"]);

testSrcProperty("Three URLs without format.",
    "url(shader.vs), url(shader.fs), url(shader3)",
    "url(shader.vs), url(shader.fs), url(shader3)",
    ["url(shader.vs)",
    "url(shader.fs)",
    "url(shader3)"]);

heading("URLs with format tests.");
testSrcProperty("One URL with format.",
    "url(shader.vs) format('x-shader/x-vertex')",
    "url(shader.vs) format('x-shader/x-vertex')",
    ["url(shader.vs) format('x-shader/x-vertex')"]);

testSrcProperty("Two URLs with format.",
    "url(shader.vs) format('x-shader/x-vertex'), url(shader.fs) format('x-shader/x-fragment')",
    "url(shader.vs) format('x-shader/x-vertex'), url(shader.fs) format('x-shader/x-fragment')",
    ["url(shader.vs) format('x-shader/x-vertex')",
    "url(shader.fs) format('x-shader/x-fragment')"]);

testSrcProperty("Three URLs with format.",
    "url(shader.vs) format('x-shader/x-vertex'), url(shader.fs) format('x-shader/x-fragment'), url(shader3) format('unknown')",
    "url(shader.vs) format('x-shader/x-vertex'), url(shader.fs) format('x-shader/x-fragment'), url(shader3) format('unknown')",
    ["url(shader.vs) format('x-shader/x-vertex')",
    "url(shader.fs) format('x-shader/x-fragment')",
    "url(shader3) format('unknown')"]);

heading("Mixed URLs with and without format tests.");
testSrcProperty("Three URLs. Middle URL without format.",
    "url(shader.vs) format('x-shader/x-vertex'), url(shader.fs), url(shader3) format('unknown')",
    "url(shader.vs) format('x-shader/x-vertex'), url(shader.fs), url(shader3) format('unknown')",
    ["url(shader.vs) format('x-shader/x-vertex')",
    "url(shader.fs)",
    "url(shader3) format('unknown')"]);

testSrcProperty("Three URLs. First and last URLs without format.",
    "url(shader.vs), url(shader.fs) format('x-shader/x-fragment'), url(shader3)",
    "url(shader.vs), url(shader.fs) format('x-shader/x-fragment'), url(shader3)",
    ["url(shader.vs)",
    "url(shader.fs) format('x-shader/x-fragment')",
    "url(shader3)"]);

heading("Capitalization tests.");
testSrcProperty("All tokens capitalized.",
    "URL(SHADER.VS) FORMAT('X-SHADER/X-VERTEX')",
    "url(SHADER.VS) format('X-SHADER/X-VERTEX')",
    ["url(SHADER.VS) format('X-SHADER/X-VERTEX')"]);

heading("Whitespace tests.");
testSrcProperty("Extra leading, middle, and trailing whitespace. ",
    "   url(shader.vs)   format('x-shader/x-vertex')   ,   url(shader.fs)   format('x-shader/x-fragment')   ",
    "url(shader.vs) format('x-shader/x-vertex'), url(shader.fs) format('x-shader/x-fragment')",
    ["url(shader.vs) format('x-shader/x-vertex')",
    "url(shader.fs) format('x-shader/x-fragment')"]);

testSrcProperty("No whitespace around comma.",
    "url(shader.vs) format('x-shader/x-vertex'),url(shader.fs) format('x-shader/x-fragment')",
    "url(shader.vs) format('x-shader/x-vertex'), url(shader.fs) format('x-shader/x-fragment')",
    ["url(shader.vs) format('x-shader/x-vertex')",
    "url(shader.fs) format('x-shader/x-fragment')"]);

testSrcProperty("No whitespace between URL and format.",
    "url(shader.vs)format('x-shader/x-vertex')",
    "url(shader.vs) format('x-shader/x-vertex')",
    ["url(shader.vs) format('x-shader/x-vertex')"]);
