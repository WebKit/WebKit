description("Tests the computed style of the custom() function of the -webkit-filter property.");

function jsWrapperClass(node)
{
    if (!node)
        return "[null]";
    var string = Object.prototype.toString.apply(node);
    return string.substr(8, string.length - 9);
}

// Need to remove the base URL to avoid having local paths in the expected results.
function removeBaseURL(src) {
    var urlRegexp = /url\(([^\)]*)\)/g;
    return src.replace(urlRegexp, function(match, url) {
        return "url(" + url.substr(url.lastIndexOf("/") + 1) + ")";
    });
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
var stylesheet, cssRule, declaration, filterRule, subRule;

function testFilterRule(description, rule, expectedValue, expectedTypes, expectedTexts)
{
    debug("");
    debug(description + " : " + rule);

    stylesheet = document.styleSheets.item(0);
    stylesheet.insertRule("body { -webkit-filter: " + rule + "; }", 0);

    filterStyle = window.getComputedStyle(document.body);
    
    shouldBe("removeBaseURL(filterStyle.getPropertyValue('-webkit-filter'))", "'" + expectedValue + "'");

    filterRule = filterStyle.getPropertyCSSValue('-webkit-filter');
    shouldBeType("filterRule", "CSSValueList");

    if (!expectedTypes) {
        expectedTypes = ["WebKitCSSFilterValue.CSS_FILTER_CUSTOM"];
        expectedTexts = [expectedValue];
    }
    
    shouldBe("filterRule.length", "" + expectedTypes.length); // shouldBe expects string arguments
  
    if (filterRule) {
        for (var i = 0; i < expectedTypes.length; i++) {
            subRule = filterRule[i];
            shouldBe("subRule.operationType", expectedTypes[i]);
            shouldBe("removeBaseURL(subRule.cssText)", "'" + expectedTexts[i] + "'");
        }
    }
    
    stylesheet.deleteRule(0);
}

testFilterRule("Custom with vertex shader",
    "custom(url(vertex.shader))", "custom(url(vertex.shader))");

testFilterRule("Custom with fragment shader",
    "custom(none url(fragment.shader))", "custom(none url(fragment.shader))");

testFilterRule("Custom with both shaders",
    "custom(url(vertex.shader) url(fragment.shader))", "custom(url(vertex.shader) url(fragment.shader))");

testFilterRule("Multiple with fragment shader",
    "grayscale() custom(none url(fragment.shader)) sepia()", "grayscale(1) custom(none url(fragment.shader)) sepia(1)",
    ["WebKitCSSFilterValue.CSS_FILTER_GRAYSCALE",
    "WebKitCSSFilterValue.CSS_FILTER_CUSTOM",
    "WebKitCSSFilterValue.CSS_FILTER_SEPIA"],
    ["grayscale(1)",
    "custom(none url(fragment.shader))",
    "sepia(1)"]);
