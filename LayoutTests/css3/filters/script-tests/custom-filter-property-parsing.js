description("Test the parsing of the custom() function of the -webkit-filter property.");

if (window.testRunner) {
    window.testRunner.overridePreference("WebKitCSSCustomFilterEnabled", "1");
    window.testRunner.overridePreference("WebKitWebGLEnabled", "1");
}

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

var styleElement = document.createElement("style");
document.head.appendChild(styleElement);

// These have to be global for the test helpers to see them.
var cssRule, declaration, filterRule, subRule;
var stylesheet = styleElement.sheet;

function testFilterRule(description, rule, expectedValue, expectedTypes, expectedTexts)
{
    debug("");
    debug(description + " : " + rule);

    stylesheet.insertRule("body { -webkit-filter: " + rule + "; }", 0);
    cssRule = stylesheet.cssRules.item(0);
  
    shouldBe("cssRule.type", "1");

    declaration = cssRule.style;
    shouldBe("declaration.length", "1");
    shouldBe("removeBaseURL(declaration.getPropertyValue('-webkit-filter'))", "'" + expectedValue + "'");

    filterRule = declaration.getPropertyCSSValue('-webkit-filter');
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
}

testFilterRule("Custom with vertex shader",
               "custom(url(vertex.shader))", "custom(url(vertex.shader))");

testFilterRule("Custom with fragment shader",
              "custom(none url(fragment.shader))", "custom(none url(fragment.shader))");

testFilterRule("Custom with both shaders",
            "custom(url(vertex.shader) url(fragment.shader))", "custom(url(vertex.shader) url(fragment.shader))");

testFilterRule("Custom with mix function",
            "custom(none mix(url(fragment.shader)))", "custom(none mix(url(fragment.shader)))");

var blendModes = ["normal", "multiply", "screen", "overlay", "darken", "lighten", "color-dodge",
            "color-burn", "hard-light", "soft-light", "difference", "exclusion", "hue", "saturation",
            "color", "luminosity"];
for (var i = 0; i < blendModes.length; i++) {
    var blendMode = blendModes[i];
    testFilterRule("Custom with mix function and blend mode " + blendMode,
                "custom(none mix(url(fragment.shader) " + blendMode + "))", "custom(none mix(url(fragment.shader) " + blendMode + "))");
}

var alphaCompositingModes = ["clear", "copy", "source-over", "destination-over", "source-in", "destination-in",
            "source-out", "destination-out", "source-atop", "destination-atop", "xor"];
for (var i = 0; i < alphaCompositingModes.length; i++) {
    var alphaCompositingMode = alphaCompositingModes[i];
    testFilterRule("Custom with mix function and alpha compositing mode " + alphaCompositingMode,
            "custom(none mix(url(fragment.shader) " + alphaCompositingMode + "))", "custom(none mix(url(fragment.shader) " + alphaCompositingMode + "))");
}

testFilterRule("Custom with mix function and blend mode first, alpha compositing mode second",
            "custom(none mix(url(fragment.shader) multiply clear))", "custom(none mix(url(fragment.shader) multiply clear))");

testFilterRule("Custom with mix function and alpha compositing mode first, blend mode second",
            "custom(none mix(url(fragment.shader) clear multiply))", "custom(none mix(url(fragment.shader) clear multiply))");

testFilterRule("Custom with vertex shader and mix function",
            "custom(url(vertex.shader) mix(url(fragment.shader)))", "custom(url(vertex.shader) mix(url(fragment.shader)))");

testFilterRule("Custom with mix function in CAPS",
            "custom(none MIX(url(fragment.shader)))", "custom(none mix(url(fragment.shader)))");

testFilterRule("Custom with mix function and mesh size",
            "custom(none mix(url(fragment.shader)), 10)", "custom(none mix(url(fragment.shader)), 10)");
            
testFilterRule("Custom with mesh size",
            "custom(url(vertex.shader), 10)", "custom(url(vertex.shader), 10)");

testFilterRule("Custom with both mesh sizes",
            "custom(none url(fragment.shader), 10 20)", "custom(none url(fragment.shader), 10 20)");

testFilterRule("Custom with detached mesh",
            "custom(none url(fragment.shader), detached)", "custom(none url(fragment.shader), detached)");
            
testFilterRule("Custom with detached and mesh size",
            "custom(none url(fragment.shader), 10 20 detached)", "custom(none url(fragment.shader), 10 20 detached)");

testFilterRule("Custom with number parameters",
            "custom(none url(fragment.shader), p1 1, p2 2 3, p3 3 4 5, p4 4 5 6 7)",
            "custom(none url(fragment.shader), p1 1, p2 2 3, p3 3 4 5, p4 4 5 6 7)");

testFilterRule("Custom with float parameters",
            "custom(none url(fragment.shader), p1 1.1, p2 2.2 3.3, p3 3.1 4.1 5.1, p4 4.1 5.2 6.3 7.4)",
            "custom(none url(fragment.shader), p1 1.1, p2 2.2 3.3, p3 3.1 4.1 5.1, p4 4.1 5.2 6.3 7.4)");

testFilterRule("Custom with parameter name same as CSS value keyword",
            "custom(none url(fragment.shader), background 0 1 0 1)",
            "custom(none url(fragment.shader), background 0 1 0 1)");

testFilterRule("Custom with mesh size and number parameters",
            "custom(none url(fragment.shader), 10 20, p1 1, p2 2 3, p3 3 4 5, p4 4 5 6 7)",
            "custom(none url(fragment.shader), 10 20, p1 1, p2 2 3, p3 3 4 5, p4 4 5 6 7)");

testFilterRule("Multiple with fragment shader",
            "grayscale() custom(none url(fragment.shader)) sepia()", "grayscale() custom(none url(fragment.shader)) sepia()",
            ["WebKitCSSFilterValue.CSS_FILTER_GRAYSCALE",
            "WebKitCSSFilterValue.CSS_FILTER_CUSTOM",
            "WebKitCSSFilterValue.CSS_FILTER_SEPIA"],
            ["grayscale()",
            "custom(none url(fragment.shader))",
            "sepia()"]);

testFilterRule("Custom in CAPS",
            "CUSTOM(url(vertex.shader))", "custom(url(vertex.shader))");

testFilterRule("Custom with one transform",
            "custom(none url(fragment.shader), oneTransform rotate(0deg))",
            "custom(none url(fragment.shader), oneTransform rotate(0deg))");

testFilterRule("Custom with multiple transforms",
            "custom(none url(fragment.shader), multipleTransform rotate(0deg) perspective(0) scale(0, 0) translate(0px, 0px))",
            "custom(none url(fragment.shader), multipleTransform rotate(0deg) perspective(0) scale(0, 0) translate(0px, 0px))");

testFilterRule("Custom with two separate transforms",
            "custom(none url(fragment.shader), firstTransform rotate(0deg), secondTransform rotate(0deg))",
            "custom(none url(fragment.shader), firstTransform rotate(0deg), secondTransform rotate(0deg))");

testFilterRule("Custom with a single element array()",
            "custom(none url(fragment.shader), t0 array(1))",
            "custom(none url(fragment.shader), t0 array(1))");


testFilterRule("Custom with a single negative element array()",
            "custom(none url(fragment.shader), t1 array(-1.01))",
            "custom(none url(fragment.shader), t1 array(-1.01))");

testFilterRule("Custom with multiple elements array()",
            "custom(none url(fragment.shader), t2 array(1, 2, 3, 4, 5))",
            "custom(none url(fragment.shader), t2 array(1, 2, 3, 4, 5))");

testFilterRule("Custom with multiple signed float elements array()",
            "custom(none url(fragment.shader), t3 array(1, -2.2, 3.14, 0.4, 5))",
            "custom(none url(fragment.shader), t3 array(1, -2.2, 3.14, 0.4, 5))");

testFilterRule("Multiple array()",
            "custom(none url(fragment.shader), t4 array(1, -2.2, 3.14, 0.4, 5), t5 array(1, 2, 3))",
            "custom(none url(fragment.shader), t4 array(1, -2.2, 3.14, 0.4, 5), t5 array(1, 2, 3))");

// TODO: The following tests are due to be removed along with the mesh box types parsing code.
// See: https://bugs.webkit.org/show_bug.cgi?id=103778
testFilterRule("Legacy - Custom with mesh box sizes and content-box",
            "custom(none url(fragment.shader), 1 1 content-box)",
            "custom(none url(fragment.shader), 1 1 content-box)");

testFilterRule("Legacy - Custom with mesh box sizes and border-box",
            "custom(none url(fragment.shader), 1 1 border-box)",
            "custom(none url(fragment.shader), 1 1 border-box)");

testFilterRule("Legacy - Custom with mesh box sizes and padding-box",
            "custom(none url(fragment.shader), 1 1 padding-box)",
            "custom(none url(fragment.shader), 1 1 padding-box)");

testFilterRule("Legacy - Custom with partial mesh box size and padding-box",
            "custom(none url(fragment.shader), 10 padding-box)",
            "custom(none url(fragment.shader), 10 padding-box)");

testFilterRule("Legacy - Custom with padding-box and detached",
            "custom(none url(fragment.shader), padding-box detached)",
            "custom(none url(fragment.shader), padding-box detached)");

testFilterRule("Legacy - Custom with both mesh-sizes and padding-box",
            "custom(none url(fragment.shader), 10 20 padding-box)",
            "custom(none url(fragment.shader), 10 20 padding-box)");

testFilterRule("Legacy - Custom with mesh-sizes, padding-box and detached",
            "custom(none url(fragment.shader), 10 20 padding-box detached)",
            "custom(none url(fragment.shader), 10 20 padding-box detached)");
