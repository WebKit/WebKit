// Requires custom-filter-parsing-common.js.

description("Test src property parsing in the @-webkit-filter at-rule.");

// These have to be global for the test helpers to see them.
var filterAtRule, styleDeclaration;

function testInvalidSrcProperty(description, propertyValue)
{
    var srcProperty = "src: " + propertyValue + ";";
    debug("\n" + description + "\n" + srcProperty);

    stylesheet.insertRule("@-webkit-filter my-filter { " + srcProperty + " }", 0);

    filterAtRule = stylesheet.cssRules.item(0);
    shouldBe("filterAtRule.type", "CSSRule.WEBKIT_FILTER_RULE");

    styleDeclaration = filterAtRule.style;
    shouldBe("styleDeclaration.length", "0");
    shouldBe("styleDeclaration.getPropertyValue('src')", "null");
}

heading("URL tests.");
testInvalidSrcProperty("No property value.", "");
testInvalidSrcProperty("Only a single comma.", ",");
testInvalidSrcProperty("Only two commas.", ", ,");
testInvalidSrcProperty("Identifier instead of URL.", "vertex-shader");
testInvalidSrcProperty("Identifier with format instead of URL with format.", "vertex-shader format('x-shader/x-vertex')");
testInvalidSrcProperty("Local function instead of URL function.", "local(shader.vs)");

heading("Format function tests.");
testInvalidSrcProperty("Identifier instead of format function.", "url(shader.vs) vertex-shader");
testInvalidSrcProperty("String instead of format function.", "url(shader.vs) 'x-shader/x-vertex'");
testInvalidSrcProperty("Format function misspelled.", "url(shader.vs) frmt('x-shader/x-vertex')");
testInvalidSrcProperty("Format function empty.", "url(shader.vs) format()");
testInvalidSrcProperty("Format function with single identifer argument.", "url(shader.vs) format(vertex-shader)");
testInvalidSrcProperty("Format function with single number argument.", "url(shader.vs) format(0)");
testInvalidSrcProperty("Format function with multiple space-separated identifer arguments.", "url(shader.vs) format(vertex-shader fragment-shader)");
testInvalidSrcProperty("Format function with multiple comma-separated identifer arguments.", "url(shader.vs) format(vertex-shader,fragment-shader)");
testInvalidSrcProperty("Format function with multiple space-separated string arguments.", "url(shader.vs) format('x-shader/x-vertex' 'x-shader/x-fragment')");
testInvalidSrcProperty("Format function with multiple comma-separated string arguments.", "url(shader.vs) format('x-shader/x-vertex','x-shader/x-fragment')");
testInvalidSrcProperty("Multiple valid format functions.", "url(shader.vs) format('x-shader/x-vertex') format('x-shader/x-fragment')");

heading("Punctuation tests.");
testInvalidSrcProperty("Leading comma before first URL.", ", url(shader.vs) format('x-shader/x-vertex')");
testInvalidSrcProperty("Comma between URL and format.", "url(shader.vs), format('x-shader/x-vertex')");
testInvalidSrcProperty("Trailing comma after format.", "url(shader.vs) format('x-shader/x-vertex'),");
testInvalidSrcProperty("Two commas between URLs.", "url(shader.vs),,url(shader.fs)");
testInvalidSrcProperty("Two commas between URLs with format.", "url(shader.vs) format('x-shader/x-vertex'),,url(shader.fs) format('x-shader/x-fragment')");
testInvalidSrcProperty("No comma between URLs.", "url(shader.vs) url(shader.fs)");
testInvalidSrcProperty("No comma between URLs with format.", "url(shader.vs) format('x-shader/x-vertex') url(shader.fs) format('x-shader/x-fragment')");
