// Helper functions.

function heading(string)
{
    debug("");
    debug("========================================");
    debug(string);
    debug("========================================");
}

function jsWrapperClass(node)
{
    if (!node)
        return "[null]";
    var string = Object.prototype.toString.apply(node);
    return string.substr(8, string.length - 9);
}

// FIXME: This type-checking approach fails on V8, which requires us to have Chromium-specific expectations.
// We should use the shouldHaveConstructor function instead, which works regardless of JS engine.
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

function shouldHaveConstructor(expression, constructorName)
{
    shouldBeTrue(expression + " instanceof " + constructorName);
    shouldBeTrue(expression + ".constructor === " + constructorName);
    shouldBeTrue(expression + ".__proto__ === " + constructorName + ".prototype");
}

// Need to remove the base URL to avoid having local paths in the expected results.
function removeBaseURL(src) 
{
    var urlRegexp = /url\(([^\)]*)\)/g;
    return src.replace(urlRegexp, function(match, url) {
        return "url(" + url.substr(url.lastIndexOf("/") + 1) + ")";
    });
}

// Common setup for parsing tests.

if (window.testRunner) {
    window.testRunner.overridePreference("WebKitCSSCustomFilterEnabled", "1");
    window.testRunner.overridePreference("WebKitWebGLEnabled", "1");
}

var styleElement = document.createElement("style");
document.head.appendChild(styleElement);
var stylesheet = styleElement.sheet;
