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
    "custom(url(vertex.shader))", "custom(url(vertex.shader) none, 1 1 filter-box)");

testFilterRule("Custom with fragment shader",
    "custom(none url(fragment.shader))", "custom(none url(fragment.shader), 1 1 filter-box)");

testFilterRule("Custom with both shaders",
    "custom(url(vertex.shader) url(fragment.shader))", "custom(url(vertex.shader) url(fragment.shader), 1 1 filter-box)");

testFilterRule("Custom with mesh size",
    "custom(url(vertex.shader), 10)", "custom(url(vertex.shader) none, 10 10 filter-box)");

testFilterRule("Custom with both mesh sizes",
    "custom(none url(fragment.shader), 10 20)", "custom(none url(fragment.shader), 10 20 filter-box)");

testFilterRule("Custom with detached mesh",
    "custom(none url(fragment.shader), detached)", "custom(none url(fragment.shader), 1 1 filter-box detached)");

testFilterRule("Custom with detached and mesh size",
    "custom(none url(fragment.shader), 10 20 detached)", "custom(none url(fragment.shader), 10 20 filter-box detached)");

testFilterRule("Custom with default filter-box",
    "custom(none url(fragment.shader), filter-box)", "custom(none url(fragment.shader), 1 1 filter-box)");

testFilterRule("Custom with content-box",
    "custom(none url(fragment.shader), content-box)", "custom(none url(fragment.shader), 1 1 content-box)");

testFilterRule("Custom with border-box",
    "custom(none url(fragment.shader), border-box)", "custom(none url(fragment.shader), 1 1 border-box)");

testFilterRule("Custom with padding-box",
    "custom(none url(fragment.shader), padding-box)", "custom(none url(fragment.shader), 1 1 padding-box)");

testFilterRule("Custom with mesh-size and padding-box",
    "custom(none url(fragment.shader), 10 padding-box)", "custom(none url(fragment.shader), 10 10 padding-box)");

testFilterRule("Custom with mesh-size and padding-box",
    "custom(none url(fragment.shader), padding-box detached)", "custom(none url(fragment.shader), 1 1 padding-box detached)");

testFilterRule("Custom with both mesh-sizes and padding-box",
    "custom(none url(fragment.shader), 10 20 padding-box)", "custom(none url(fragment.shader), 10 20 padding-box)");

testFilterRule("Custom with both mesh-sizes and padding-box and detached",
    "custom(none url(fragment.shader), 10 20 padding-box detached)", "custom(none url(fragment.shader), 10 20 padding-box detached)");

testFilterRule("Custom with padding-box and detached",
    "custom(none url(fragment.shader), padding-box detached)", "custom(none url(fragment.shader), 1 1 padding-box detached)");

testFilterRule("Custom with integer parameters",
            "custom(none url(fragment.shader), p1 1, p2 2 3, p3 3 4 5, p4 4 5 6 7)",
            "custom(none url(fragment.shader), 1 1 filter-box, p1 1, p2 2 3, p3 3 4 5, p4 4 5 6 7)");

testFilterRule("Custom with float parameters",
            "custom(none url(fragment.shader), p1 1.1, p2 2.2 3.3, p3 3.1 4.1 5.1, p4 4.1 5.2 6.3 7.4)",
            "custom(none url(fragment.shader), 1 1 filter-box, p1 1.1, p2 2.2 3.3, p3 3.1 4.1 5.1, p4 4.1 5.2 6.3 7.4)");

testFilterRule("Custom with mesh size and number parameters",
            "custom(none url(fragment.shader), 10 20 filter-box, p1 1, p2 2 3, p3 3 4 5, p4 4 5 6 7)",
            "custom(none url(fragment.shader), 10 20 filter-box, p1 1, p2 2 3, p3 3 4 5, p4 4 5 6 7)");

testFilterRule("Multiple with fragment shader",
    "grayscale() custom(none url(fragment.shader)) sepia()", "grayscale(1) custom(none url(fragment.shader), 1 1 filter-box) sepia(1)",
    ["WebKitCSSFilterValue.CSS_FILTER_GRAYSCALE",
    "WebKitCSSFilterValue.CSS_FILTER_CUSTOM",
    "WebKitCSSFilterValue.CSS_FILTER_SEPIA"],
    ["grayscale(1)",
    "custom(none url(fragment.shader), 1 1 filter-box)",
    "sepia(1)"]);
