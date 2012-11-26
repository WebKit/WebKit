description("Test mix() function JS bindings.");

function jsWrapperClass(node) {
    if (!node)
        return "[null]";
    var string = Object.prototype.toString.apply(node);
    return string.substr(8, string.length - 9);
}

function shouldBeType(expression, className, prototypeName, constructorName) {
    if (!prototypeName)
        prototypeName = className + "Prototype";
    if (!constructorName)
        constructorName = className + "Constructor";
    shouldBe("jsWrapperClass(" + expression + ")", "'" + className + "'");
    shouldBe("jsWrapperClass(" + expression + ".__proto__)", "'" + prototypeName + "'");
    shouldBe("jsWrapperClass(" + expression + ".constructor)", "'" + constructorName + "'");
}

function removeBaseURL(src) {
    var urlRegexp = /url\(([^\)]*)\)/g;
    return src.replace(urlRegexp, function(match, url) {
        return "url(" + url.substr(url.lastIndexOf("/") + 1) + ")";
    });
}

if (window.testRunner) {
    window.testRunner.overridePreference("WebKitCSSCustomFilterEnabled", "1");
    window.testRunner.overridePreference("WebKitWebGLEnabled", "1");
}

var styleElement = document.createElement("style");
document.head.appendChild(styleElement);
var stylesheet = styleElement.sheet;

stylesheet.addRule("body", "-webkit-filter: custom(none mix(url('../resources/color-offset.fs')))", 0);

var cssRule = stylesheet.cssRules.item(0);

shouldBe("cssRule.type", "1");

var declaration = cssRule.style;
shouldBe("declaration.length", "1");

var filterRule = declaration.getPropertyCSSValue('-webkit-filter');
var mixFunction = filterRule[0][0][1];
shouldBeType("mixFunction", "WebKitCSSMixFunctionValue");

var mixFunctionContent = removeBaseURL(mixFunction.cssText);

shouldBeEqualToString("mixFunctionContent", "mix(url(color-offset.fs))");

successfullyParsed = true;
