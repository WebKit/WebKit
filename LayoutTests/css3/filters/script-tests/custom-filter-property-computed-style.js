description("Tests the computed style of the custom() function of the -webkit-filter property.");

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

// These have to be global for the test helpers to see them.
var stylesheet, cssRule, declaration, filterRule, subRule;
var styleElement = document.createElement("style");
document.head.appendChild(styleElement);
stylesheet = styleElement.sheet;

// We need a fixed width/height for the object.
var objectToCheck = document.createElement("div");
objectToCheck.className = "objectToCheck";
objectToCheck.style.width = "500px";
objectToCheck.style.height = "600px";
document.body.appendChild(objectToCheck);

function testFilterRule(description, rule, expectedValue, expectedTypes, expectedTexts)
{
    debug("");
    debug(description + " : " + rule);

    stylesheet.insertRule(".objectToCheck { -webkit-filter: " + rule + "; }", 0);

    filterStyle = window.getComputedStyle(objectToCheck);
    
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
    "custom(url(vertex.shader))", "custom(url(vertex.shader) none, 1 1)");

testFilterRule("Custom with fragment shader",
    "custom(none url(fragment.shader))", "custom(none url(fragment.shader), 1 1)");

testFilterRule("Custom with both shaders",
    "custom(url(vertex.shader) url(fragment.shader))", "custom(url(vertex.shader) url(fragment.shader), 1 1)");

testFilterRule("Custom with mix function",
    "custom(none mix(url(fragment.shader)))", "custom(none mix(url(fragment.shader) normal source-atop), 1 1)");

var blendModes = ["normal", "multiply", "screen", "overlay", "darken", "lighten", "color-dodge",
                  "color-burn", "hard-light", "soft-light", "difference", "exclusion", "hue",
                  "saturation", "color", "luminosity"];
for (var i = 0; i < blendModes.length; i++) {
    var blendMode = blendModes[i];
    testFilterRule("Custom with mix function and blend mode " + blendMode,
        "custom(none mix(url(fragment.shader) " + blendMode + "))", "custom(none mix(url(fragment.shader) " + blendMode + " source-atop), 1 1)");
}

var alphaCompositingModes = ["clear", "copy", "source-over", "destination-over", "source-in", "destination-in",
        "source-out", "destination-out", "source-atop", "destination-atop", "xor"];
for (var i = 0; i < alphaCompositingModes.length; i++) {
    var alphaCompositingMode = alphaCompositingModes[i];
    testFilterRule("Custom with mix function and alpha compositing mode " + alphaCompositingMode,
        "custom(none mix(url(fragment.shader) " + alphaCompositingMode + "))", "custom(none mix(url(fragment.shader) normal " + alphaCompositingMode + "), 1 1)");
}

testFilterRule("Custom with mix function and blend mode first, alpha compositing mode second",
    "custom(none mix(url(fragment.shader) multiply clear))", "custom(none mix(url(fragment.shader) multiply clear), 1 1)");

testFilterRule("Custom with mix function and alpha compositing mode first, blend mode second",
    "custom(none mix(url(fragment.shader) clear multiply))", "custom(none mix(url(fragment.shader) multiply clear), 1 1)");

testFilterRule("Custom with vertex shader and mix function",
    "custom(url(vertex.shader) mix(url(fragment.shader)))", "custom(url(vertex.shader) mix(url(fragment.shader) normal source-atop), 1 1)");

testFilterRule("Custom with mix function and mesh size",
    "custom(none mix(url(fragment.shader)), 10)", "custom(none mix(url(fragment.shader) normal source-atop), 10 10)");

testFilterRule("Custom with mesh size",
    "custom(url(vertex.shader), 10)", "custom(url(vertex.shader) none, 10 10)");

testFilterRule("Custom with both mesh sizes",
    "custom(none url(fragment.shader), 10 20)", "custom(none url(fragment.shader), 10 20)");

testFilterRule("Custom with mesh size",
    "custom(url(vertex.shader), 10)", "custom(url(vertex.shader) none, 10 10)");

testFilterRule("Custom with both mesh sizes",
    "custom(none url(fragment.shader), 10 20)", "custom(none url(fragment.shader), 10 20)");

testFilterRule("Custom with detached mesh",
    "custom(none url(fragment.shader), detached)", "custom(none url(fragment.shader), 1 1 detached)");

testFilterRule("Custom with detached and mesh size",
    "custom(none url(fragment.shader), 10 20 detached)", "custom(none url(fragment.shader), 10 20 detached)");

testFilterRule("Custom with default filter-box",
    "custom(none url(fragment.shader))", "custom(none url(fragment.shader), 1 1)");

testFilterRule("Custom with integer parameters",
            "custom(none url(fragment.shader), p1 1, p2 2 3, p3 3 4 5, p4 4 5 6 7)",
            "custom(none url(fragment.shader), 1 1, p1 1, p2 2 3, p3 3 4 5, p4 4 5 6 7)");

testFilterRule("Custom with float parameters",
            "custom(none url(fragment.shader), p1 1.1, p2 2.2 3.3, p3 3.1 4.1 5.1, p4 4.1 5.2 6.3 7.4)",
            "custom(none url(fragment.shader), 1 1, p1 1.1, p2 2.2 3.3, p3 3.1 4.1 5.1, p4 4.1 5.2 6.3 7.4)");

testFilterRule("Custom with transform translate parameter",
            "custom(none url(fragment.shader), p1 translate(10px, 10px))",
            "custom(none url(fragment.shader), 1 1, p1 matrix(1, 0, 0, 1, 10, 10))");

testFilterRule("Custom with multiple transform parameters",
            "custom(none url(fragment.shader), p1 translate(10px, 10px), p2 scale(2))",
            "custom(none url(fragment.shader), 1 1, p1 matrix(1, 0, 0, 1, 10, 10), p2 matrix(2, 0, 0, 2, 0, 0))");

testFilterRule("Custom with multiple transforms single parameter",
            "custom(none url(fragment.shader), p1 translate(10px, 10px) scale(2))",
            "custom(none url(fragment.shader), 1 1, p1 matrix(2, 0, 0, 2, 10, 10))");

testFilterRule("Custom with percent transform parameter",
            "custom(none url(fragment.shader), p1 translate(50%, 50%))",
            "custom(none url(fragment.shader), 1 1, p1 matrix(1, 0, 0, 1, 250, 300))");

testFilterRule("Custom with mesh size and number parameters",
            "custom(none url(fragment.shader), 10 20, p1 1, p2 2 3, p3 3 4 5, p4 4 5 6 7)",
            "custom(none url(fragment.shader), 10 20, p1 1, p2 2 3, p3 3 4 5, p4 4 5 6 7)");

testFilterRule("Multiple with fragment shader",
    "grayscale() custom(none url(fragment.shader)) sepia()", "grayscale(1) custom(none url(fragment.shader), 1 1) sepia(1)",
    ["WebKitCSSFilterValue.CSS_FILTER_GRAYSCALE",
    "WebKitCSSFilterValue.CSS_FILTER_CUSTOM",
    "WebKitCSSFilterValue.CSS_FILTER_SEPIA"],
    ["grayscale(1)",
    "custom(none url(fragment.shader), 1 1)",
    "sepia(1)"]);

testFilterRule("Custom with array() made up of three values",
            "custom(none url(fragment.shader), a1 array(1, 2, 3))",
            "custom(none url(fragment.shader), 1 1, a1 array(1, 2, 3))");

testFilterRule("Custom with array() made up of three floating point values",
            "custom(none url(fragment.shader), a1 array(1.6180, -2.7182, 3.1415))",
            "custom(none url(fragment.shader), 1 1, a1 array(1.618, -2.7182, 3.1415))");

testFilterRule("Custom with array() made up of several floating point and integer values",
            "custom(none url(fragment.shader), a1 array(1.6180, 1, -2.7182, 0.1, 3.1415))",
            "custom(none url(fragment.shader), 1 1, a1 array(1.618, 1, -2.7182, 0.1, 3.1415))");

testFilterRule("Custom with array() made up of 247 different floating point values",
            "custom(none url(fragment.shader), a1 array(6.00, 38.58, 40.14, 34.03, 83.33, 24.18, 68.37, 15.87, 56.56, 73.21, 99.05, 24.64, 6.39, 69.34, 91.19, 27.98, 37.52, 60.71, 21.94, 62.70, 77.96, 67.27, 2.75, 28.16, 8.88, 11.61, 34.89, 45.75, 54.55, 35.04, 6.13, 69.68, 81.21, 49.44, 54.74, 26.74, 91.49, 68.20, 84.05, 65.81, 57.89, 73.22, 13.87, 71.73, 18.61, 8.54, 9.79, 28.05, 14.11, 69.93, 47.19, 10.48, 81.35, 37.15, 19.29, 14.98, 32.54, 98.96, 69.37, 92.26, 34.64, 67.43, 71.92, 45.52, 9.22, 41.96, 31.26, 92.65, 35.02, 39.00, 76.76, 29.33, 22.93, 80.65, 70.78, 42.94, 27.72, 28.19, 75.15, 18.14, 89.71, 95.38, 54.51, 36.93, 98.45, 87.58, 44.51, 71.73, 75.71, 70.43, 78.51, 82.19, 31.73, 2.90, 8.23, 93.74, 31.26, 84.73, 6.48, 41.96, 75.72, 53.96, 35.06, 67.50, 76.62, 35.02, 1.62, 13.05, 33.59, 26.89, 36.94, 74.13, 95.09, 56.87, 46.01, 11.94, 12.92, 48.88, 11.03, 24.80, 68.06, 19.83, 12.98, 41.56, 62.03, 80.35, 41.10, 5.31, 43.96, 9.02, 92.32, 34.50, 88.41, 90.34, 97.62, 58.70, 23.62, 13.03, 85.41, 29.73, 30.21, 59.25, 91.22, 78.21, 62.30, 60.93, 33.26, 16.13, 95.61, 78.62, 97.42, 96.42, 38.04, 8.49, 45.47, 68.73, 18.20, 26.65, 78.39, 95.26, 66.80, 34.41, 10.69, 94.43, 71.69, 4.58, 23.87, 61.01, 95.68, 8.51, 14.92, 16.27, 30.41, 1.40, 12.35, 99.39, 5.80, 68.10, 73.87, 28.36, 24.98, 77.58, 8.73, 34.08, 54.45, 87.64, 36.14, 74.68, 38.04, 56.54, 46.42, 12.40, 5.90, 15.70, 10.88, 55.82, 84.66, 75.74, 87.92, 59.03, 60.30, 4.32, 7.05, 3.34, 9.87, 64.51, 4.60, 54.16, 48.80, 46.93, 5.84, 90.34, 75.48, 32.59, 29.65, 94.57, 28.22, 93.70, 23.79, 89.57, 14.84, 79.58, 62.29, 58.76, 29.43, 19.16, 44.47, 38.31, 44.77, 42.66, 88.85, 74.96, 24.07, 37.75, 41.65, 62.35, 90.40, 40.77, 53.93, 80.23, 84.49, 20.18, 45.36, 26.02, 93.72, 65.54, 32.86))",
            "custom(none url(fragment.shader), 1 1, a1 array(6, 38.58, 40.14, 34.03, 83.33, 24.18, 68.37, 15.87, 56.56, 73.21, 99.05, 24.64, 6.39, 69.34, 91.19, 27.98, 37.52, 60.71, 21.94, 62.7, 77.96, 67.27, 2.75, 28.16, 8.88, 11.61, 34.89, 45.75, 54.55, 35.04, 6.13, 69.68, 81.21, 49.44, 54.74, 26.74, 91.49, 68.2, 84.05, 65.81, 57.89, 73.22, 13.87, 71.73, 18.61, 8.54, 9.79, 28.05, 14.11, 69.93, 47.19, 10.48, 81.35, 37.15, 19.29, 14.98, 32.54, 98.96, 69.37, 92.26, 34.64, 67.43, 71.92, 45.52, 9.22, 41.96, 31.26, 92.65, 35.02, 39, 76.76, 29.33, 22.93, 80.65, 70.78, 42.94, 27.72, 28.19, 75.15, 18.14, 89.71, 95.38, 54.51, 36.93, 98.45, 87.58, 44.51, 71.73, 75.71, 70.43, 78.51, 82.19, 31.73, 2.9, 8.23, 93.74, 31.26, 84.73, 6.48, 41.96, 75.72, 53.96, 35.06, 67.5, 76.62, 35.02, 1.62, 13.05, 33.59, 26.89, 36.94, 74.13, 95.09, 56.87, 46.01, 11.94, 12.92, 48.88, 11.03, 24.8, 68.06, 19.83, 12.98, 41.56, 62.03, 80.35, 41.1, 5.31, 43.96, 9.02, 92.32, 34.5, 88.41, 90.34, 97.62, 58.7, 23.62, 13.03, 85.41, 29.73, 30.21, 59.25, 91.22, 78.21, 62.3, 60.93, 33.26, 16.13, 95.61, 78.62, 97.42, 96.42, 38.04, 8.49, 45.47, 68.73, 18.2, 26.65, 78.39, 95.26, 66.8, 34.41, 10.69, 94.43, 71.69, 4.58, 23.87, 61.01, 95.68, 8.51, 14.92, 16.27, 30.41, 1.4, 12.35, 99.39, 5.8, 68.1, 73.87, 28.36, 24.98, 77.58, 8.73, 34.08, 54.45, 87.64, 36.14, 74.68, 38.04, 56.54, 46.42, 12.4, 5.9, 15.7, 10.88, 55.82, 84.66, 75.74, 87.92, 59.03, 60.3, 4.32, 7.05, 3.34, 9.87, 64.51, 4.6, 54.16, 48.8, 46.93, 5.84, 90.34, 75.48, 32.59, 29.65, 94.57, 28.22, 93.7, 23.79, 89.57, 14.84, 79.58, 62.29, 58.76, 29.43, 19.16, 44.47, 38.31, 44.77, 42.66, 88.85, 74.96, 24.07, 37.75, 41.65, 62.35, 90.4, 40.77, 53.93, 80.23, 84.49, 20.18, 45.36, 26.02, 93.72, 65.54, 32.86))");

testFilterRule("Custom with two array() made up of three and six values",
            "custom(none url(fragment.shader), a1 array(1, 2, 3), a2 array(1, 2, 3, 4, 5, 6))",
            "custom(none url(fragment.shader), 1 1, a1 array(1, 2, 3), a2 array(1, 2, 3, 4, 5, 6))");
